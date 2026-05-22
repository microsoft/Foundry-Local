// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// Translation performance benchmark: runs zh<->en translations on Foundry
// Local CUDA/WebGPU/CPU variants and optionally llama.cpp (GGUF) GPU/CPU,
// collects per-item timing, GPU memory, and CPU/GPU utilisation metrics,
// then prints a side-by-side comparison table.
//
// Build:
//   cmake --preset x64-release && cmake --build --preset x64-release
// Run:
//   translation-perf-benchmark [model-alias] [path-to.gguf]

#include "foundry_local.h"

#ifdef HAS_LLAMA_CPP
#include "llama.h"
#include "ggml.h"
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#else
#include <unistd.h>
#endif

using namespace foundry_local;
using Clock = std::chrono::high_resolution_clock;

// ---------------------------------------------------------------------------
// Logger (quiet — only errors)
// ---------------------------------------------------------------------------
class QuietLogger final : public ILogger {
public:
    void Log(LogLevel level, std::string_view message) noexcept override {
        if (level == LogLevel::Error) {
            std::cerr << "[ERROR] " << message << "\n";
        }
    }
};

// ---------------------------------------------------------------------------
// Helpers: nvidia-smi query, process RSS
// ---------------------------------------------------------------------------

/// Run `nvidia-smi --query-gpu=<field>` and return the first numeric value.
static std::optional<double> NvidiaSmiQuery(const std::string& field) {
    std::string cmd = "nvidia-smi --query-gpu=" + field +
                      " --format=csv,noheader,nounits";
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return std::nullopt;
    char buf[128]{};
    std::string output;
    while (fgets(buf, sizeof(buf), pipe))
        output += buf;
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    try { return std::stod(output); }
    catch (...) { return std::nullopt; }
}

/// Return current-process RSS in MB.
static std::optional<double> GetProcessRssMb() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    return std::nullopt;
#else
    std::ifstream ifs("/proc/self/statm");
    unsigned long pages = 0;
    if (ifs >> pages) {
        long page_sz = sysconf(_SC_PAGESIZE);
        return static_cast<double>(pages * page_sz) / (1024.0 * 1024.0);
    }
    return std::nullopt;
#endif
}

// ---------------------------------------------------------------------------
// ResourceMonitor — background sampler for GPU util% and GPU mem (MB).
// Uses a streaming `nvidia-smi -lms` subprocess for efficiency.
// ---------------------------------------------------------------------------
class ResourceMonitor {
public:
    explicit ResourceMonitor(int gpu_interval_ms = 100, int cpu_interval_ms = 100)
        : gpu_interval_ms_(gpu_interval_ms), cpu_interval_ms_(cpu_interval_ms) {}

    void Start(bool enable_gpu = true) {
        stop_.store(false);
        enable_gpu_ = enable_gpu;
        gpu_util_samples_.clear();
        gpu_mem_samples_.clear();
        cpu_samples_.clear();

        if (enable_gpu_)
            gpu_thread_ = std::thread([this] { RunGpu(); });
        cpu_thread_ = std::thread([this] { RunCpu(); });

        // Let nvidia-smi start streaming, then discard warmup samples
        if (enable_gpu_)
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        {
            std::lock_guard<std::mutex> lk(mu_);
            gpu_util_samples_.clear();
            gpu_mem_samples_.clear();
            cpu_samples_.clear();
        }
    }

    void Stop() {
        stop_.store(true);
#ifdef _WIN32
        if (gpu_proc_) {
            TerminateProcess(gpu_proc_, 0);
            CloseHandle(gpu_proc_);
            gpu_proc_ = nullptr;
        }
        if (gpu_pipe_rd_) {
            CloseHandle(gpu_pipe_rd_);
            gpu_pipe_rd_ = nullptr;
        }
#else
        if (gpu_pipe_) {
            pclose(gpu_pipe_);
            gpu_pipe_ = nullptr;
        }
#endif
        if (gpu_thread_.joinable()) gpu_thread_.join();
        if (cpu_thread_.joinable()) cpu_thread_.join();
    }

    struct Summary {
        std::optional<double> cpu_avg_pct;
        std::optional<double> gpu_util_avg_pct;
        std::optional<double> gpu_util_peak_pct;
        std::optional<double> gpu_mem_peak_mb;
    };

    Summary GetSummary() const {
        std::lock_guard<std::mutex> lk(mu_);
        Summary s;
        if (!cpu_samples_.empty()) {
            double sum = std::accumulate(cpu_samples_.begin(), cpu_samples_.end(), 0.0);
            s.cpu_avg_pct = sum / cpu_samples_.size();
        }
        if (!gpu_util_samples_.empty()) {
            double sum = std::accumulate(gpu_util_samples_.begin(),
                                         gpu_util_samples_.end(), 0.0);
            s.gpu_util_avg_pct = sum / gpu_util_samples_.size();
            s.gpu_util_peak_pct = *std::max_element(gpu_util_samples_.begin(),
                                                     gpu_util_samples_.end());
        }
        if (!gpu_mem_samples_.empty()) {
            s.gpu_mem_peak_mb = *std::max_element(gpu_mem_samples_.begin(),
                                                   gpu_mem_samples_.end());
        }
        return s;
    }

private:
    int gpu_interval_ms_;
    int cpu_interval_ms_;
    bool enable_gpu_ = true;
    std::atomic<bool> stop_{false};
    std::thread gpu_thread_;
    std::thread cpu_thread_;
    mutable std::mutex mu_;
    std::vector<double> gpu_util_samples_;
    std::vector<double> gpu_mem_samples_;
    std::vector<double> cpu_samples_;

#ifdef _WIN32
    static double FileTimeToMs(const FILETIME& ft) {
        ULARGE_INTEGER li;
        li.LowPart = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        return static_cast<double>(li.QuadPart) / 10000.0; // 100-ns units to ms
    }

    void RunCpu() {
        HANDLE hProc = GetCurrentProcess();
        FILETIME creation, exit, prevKernel, prevUser;
        GetProcessTimes(hProc, &creation, &exit, &prevKernel, &prevUser);
        auto prevWall = Clock::now();
        int numCpus = static_cast<int>(std::thread::hardware_concurrency());
        if (numCpus < 1) numCpus = 1;

        while (!stop_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(cpu_interval_ms_));
            FILETIME c2, e2, curKernel, curUser;
            GetProcessTimes(hProc, &c2, &e2, &curKernel, &curUser);
            auto curWall = Clock::now();

            double cpuMs = (FileTimeToMs(curKernel) - FileTimeToMs(prevKernel))
                         + (FileTimeToMs(curUser) - FileTimeToMs(prevUser));
            double wallMs = std::chrono::duration<double, std::milli>(curWall - prevWall).count();

            if (wallMs > 0) {
                // Normalize: cpuMs can exceed wallMs on multi-core
                double pct = (cpuMs / wallMs / numCpus) * 100.0;
                std::lock_guard<std::mutex> lk(mu_);
                cpu_samples_.push_back(pct);
            }

            prevKernel = curKernel;
            prevUser   = curUser;
            prevWall   = curWall;
        }
    }
#else
    void RunCpu() {
        // Not implemented on non-Windows; samples remain empty.
    }
#endif

#ifdef _WIN32
    HANDLE gpu_proc_ = nullptr;
    HANDLE gpu_pipe_rd_ = nullptr;

    void RunGpu() {
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE rd = nullptr, wr = nullptr;
        if (!CreatePipe(&rd, &wr, &sa, 0)) return;
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

        std::string cmd = "nvidia-smi --query-gpu=utilization.gpu,memory.used "
                          "--format=csv,noheader,nounits -lms=" +
                          std::to_string(gpu_interval_ms_);

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = wr;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi{};
        if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                            nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                            nullptr, nullptr, &si, &pi)) {
            CloseHandle(rd);
            CloseHandle(wr);
            return;
        }
        CloseHandle(wr);
        CloseHandle(pi.hThread);
        gpu_proc_ = pi.hProcess;
        gpu_pipe_rd_ = rd;

        std::string line;
        char buf[256];
        DWORD bytesRead = 0;
        while (!stop_.load()) {
            if (!ReadFile(rd, buf, sizeof(buf) - 1, &bytesRead, nullptr) || bytesRead == 0)
                break;
            buf[bytesRead] = '\0';
            line += buf;
            size_t pos;
            while ((pos = line.find('\n')) != std::string::npos) {
                std::string row = line.substr(0, pos);
                line.erase(0, pos + 1);
                ParseGpuLine(row);
            }
        }
    }
#else
    FILE* gpu_pipe_ = nullptr;

    void RunGpu() {
        std::string cmd = "nvidia-smi --query-gpu=utilization.gpu,memory.used "
                          "--format=csv,noheader,nounits -lms=" +
                          std::to_string(gpu_interval_ms_);
        gpu_pipe_ = popen(cmd.c_str(), "r");
        if (!gpu_pipe_) return;
        char buf[256];
        while (!stop_.load() && fgets(buf, sizeof(buf), gpu_pipe_)) {
            ParseGpuLine(std::string(buf));
        }
    }
#endif

    void ParseGpuLine(const std::string& row) {
        auto pos = row.find(',');
        if (pos == std::string::npos) return;
        try {
            double util = std::stod(row.substr(0, pos));
            double mem  = std::stod(row.substr(pos + 1));
            std::lock_guard<std::mutex> lk(mu_);
            gpu_util_samples_.push_back(util);
            gpu_mem_samples_.push_back(mem);
        } catch (...) {}
    }
};

// ---------------------------------------------------------------------------
// Translation item
// ---------------------------------------------------------------------------
struct TranslationItem {
    int id;
    std::string direction;   // "zh->en" or "en->zh"
    std::string source;
};

// ---------------------------------------------------------------------------
// Per-item result with timing
// ---------------------------------------------------------------------------
struct TranslationResult {
    std::string text;
    double elapsed_ms;
};

// ---------------------------------------------------------------------------
// Aggregate metrics for one variant run
// ---------------------------------------------------------------------------
struct VariantMetrics {
    std::string label;
    std::string group;          // e.g. "Foundry Local 2B text" or "llama.cpp"
    std::string ep_name;
    std::string device_type;
    std::optional<int> model_size_mb;
    std::string runtime_dep_size;  // pre-formatted string, e.g. "2.25 GB + 125 MB (FL dll)"

    std::vector<TranslationResult> results;

    double total_ms = 0;
    double avg_ms   = 0;
    double min_ms   = 0;
    double max_ms   = 0;

    std::optional<double> rss_mb;           // absolute RSS after model load
    std::optional<double> vram_mb;          // absolute VRAM after model load
    std::optional<double> cpu_avg_pct;
    std::optional<double> gpu_util_avg_pct;
    std::optional<double> gpu_util_peak_pct;
};

// ---------------------------------------------------------------------------
// Translate a single text and measure wall-clock time
// ---------------------------------------------------------------------------
static TranslationResult TranslateOne(OpenAIChatClient& chat,
                                       const std::string& direction,
                                       const std::string& sourceText) {
    std::string systemPrompt, userPrompt;
    if (direction == "zh->en") {
        systemPrompt = "You are a translation assistant. Translate the user's Chinese text to English. "
                       "Respond with only the translated text, no explanations.";
        userPrompt = "Translate the following Chinese to English: " + sourceText;
    } else {
        systemPrompt = "You are a translation assistant. Translate the user's English text to Chinese. "
                       "Respond with only the translated Chinese text, no explanations.";
        userPrompt = "Translate the following English to Chinese: " + sourceText;
    }

    std::vector<ChatMessage> messages = {
        {"system", systemPrompt},
        {"user",   userPrompt}
    };

    ChatSettings settings;
    settings.temperature = 0.0f;
    settings.max_tokens  = 100;

    auto t0 = Clock::now();
    auto response = chat.CompleteChat(messages, settings);
    double elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    std::string text = "(no response)";
    if (!response.choices.empty() && response.choices[0].message) {
        text = response.choices[0].message->content;
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.pop_back();
    }

    return {std::move(text), elapsed_ms};
}

// ---------------------------------------------------------------------------
// llama.cpp backend
// ---------------------------------------------------------------------------
#ifdef HAS_LLAMA_CPP

// Forward declaration (defined below in formatting helpers)
static double DirDllSizeMb(const std::string& dir);

/// Build the chat prompt string for llama.cpp using llama_chat_apply_template.
static std::string BuildLlamaCppPrompt(const llama_model* model,
                                       const std::string& direction,
                                       const std::string& sourceText) {
    std::string systemPrompt, userPrompt;
    if (direction == "zh->en") {
        systemPrompt = "You are a translation assistant. Translate the user's Chinese text to English. "
                       "Respond with only the translated text, no explanations.";
        userPrompt = "Translate the following Chinese to English: " + sourceText;
    } else {
        systemPrompt = "You are a translation assistant. Translate the user's English text to Chinese. "
                       "Respond with only the translated Chinese text, no explanations.";
        userPrompt = "Translate the following English to Chinese: " + sourceText;
    }

    std::vector<llama_chat_message> msgs = {
        {"system", systemPrompt.c_str()},
        {"user",   userPrompt.c_str()},
    };

    // First call to get required buffer size
    int32_t needed = llama_chat_apply_template(
        nullptr, msgs.data(), msgs.size(), true, nullptr, 0);
    if (needed <= 0) {
        // Fallback: manual Qwen/ChatML template
        return "<|im_start|>system\n" + systemPrompt + "<|im_end|>\n"
               "<|im_start|>user\n" + userPrompt + "<|im_end|>\n"
               "<|im_start|>assistant\n";
    }
    std::string buf(static_cast<size_t>(needed) + 1, '\0');
    llama_chat_apply_template(
        nullptr, msgs.data(), msgs.size(), true, buf.data(), static_cast<int32_t>(buf.size()));
    buf.resize(static_cast<size_t>(needed));
    return buf;
}

/// Translate a single text using llama.cpp and return result + timing.
static TranslationResult TranslateOneLlamaCpp(llama_model* model,
                                               llama_context* ctx,
                                               llama_sampler* smpl,
                                               const std::string& direction,
                                               const std::string& sourceText,
                                               int max_tokens = 100) {
    const llama_vocab* vocab = llama_model_get_vocab(model);
    std::string prompt = BuildLlamaCppPrompt(model, direction, sourceText);

    // Tokenize
    int n_prompt = llama_tokenize(vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()),
                                  nullptr, 0, true, true);
    if (n_prompt < 0) n_prompt = -n_prompt;
    std::vector<llama_token> tokens(n_prompt);
    llama_tokenize(vocab, prompt.c_str(), static_cast<int32_t>(prompt.size()),
                   tokens.data(), n_prompt, true, true);

    // Clear KV cache for a fresh generation
    llama_memory_clear(llama_get_memory(ctx), true);
    llama_sampler_reset(smpl);

    auto t0 = Clock::now();

    // Prefill
    llama_batch batch = llama_batch_get_one(tokens.data(), n_prompt);
    if (llama_decode(ctx, batch) != 0) {
        double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        return {"(decode failed)", elapsed};
    }

    // Decode loop
    std::string output;
    for (int i = 0; i < max_tokens; ++i) {
        llama_token new_token = llama_sampler_sample(smpl, ctx, -1);

        if (llama_vocab_is_eog(vocab, new_token))
            break;

        // Detokenize
        char piece[256];
        int n = llama_token_to_piece(vocab, new_token, piece, sizeof(piece), 0, true);
        if (n > 0) output.append(piece, static_cast<size_t>(n));

        // Prepare next decode step
        batch = llama_batch_get_one(&new_token, 1);
        if (llama_decode(ctx, batch) != 0) break;
    }

    double elapsed_ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

    // Strip trailing whitespace/newlines
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' '))
        output.pop_back();

    return {std::move(output), elapsed_ms};
}

/// Run all translations using llama.cpp with specified GPU layer count.
static VariantMetrics RunLlamaCpp(const std::string& ggufPath,
                                  const std::vector<TranslationItem>& items,
                                  const std::string& label,
                                  int n_gpu_layers) {
    VariantMetrics vm;
    vm.label = label;
    vm.ep_name = (n_gpu_layers == 0) ? "CPU" : "CUDA (llama.cpp)";
    vm.device_type = (n_gpu_layers == 0) ? "CPU" : "GPU";

    // Model file size
    {
        std::ifstream f(ggufPath, std::ios::binary | std::ios::ate);
        if (f) vm.model_size_mb = static_cast<int>(f.tellg() / (1024 * 1024));
    }

    // Runtime dependency size — llama.cpp DLLs
#ifdef HAS_LLAMA_CPP
    {
        // The DLLs are next to the executable
        char exePath[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        double llamaDlls = DirDllSizeMb(exeDir.string());
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f MB", llamaDlls);
        vm.runtime_dep_size = buf;
    }
#else
    vm.runtime_dep_size = "/";
#endif

    std::cout << "Loading " << label << " (" << ggufPath << ")...\n";

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = n_gpu_layers;
    llama_model* model = llama_model_load_from_file(ggufPath.c_str(), mparams);
    if (!model) {
        std::cerr << "Failed to load GGUF model: " << ggufPath << "\n";
        return vm;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx   = 2048;
    cparams.n_batch = 2048;
    llama_context* ctx = llama_init_from_model(model, cparams);
    if (!ctx) {
        std::cerr << "Failed to create llama context\n";
        llama_model_free(model);
        return vm;
    }

    // Greedy sampler (temperature=0 equivalent)
    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    sparams.no_perf = true;
    llama_sampler* smpl = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    // Warmup
    std::cout << "  [" << label << "] warmup..." << std::flush;
    (void)TranslateOneLlamaCpp(model, ctx, smpl, "en->zh", "Hello", 10);
    std::cout << " done\n";

    // Snapshot absolute memory after model load + warmup
    vm.rss_mb  = GetProcessRssMb();
    vm.vram_mb = (n_gpu_layers != 0) ? NvidiaSmiQuery("memory.used") : std::nullopt;

    bool useGpu = (n_gpu_layers != 0);
    ResourceMonitor monitor(100);
    monitor.Start(useGpu);

    vm.results.reserve(items.size());
    for (const auto& item : items) {
        std::cout << "  [" << label << "] #" << item.id << " " << item.direction
                  << " \"" << item.source << "\" ... " << std::flush;
        auto res = TranslateOneLlamaCpp(model, ctx, smpl, item.direction, item.source);
        std::cout << res.text << "  (" << std::fixed << std::setprecision(0)
                  << res.elapsed_ms << " ms)\n";
        vm.results.push_back(std::move(res));
    }

    monitor.Stop();

    // Aggregate timing
    if (!vm.results.empty()) {
        double sum = 0, lo = vm.results[0].elapsed_ms, hi = lo;
        for (const auto& r : vm.results) {
            sum += r.elapsed_ms;
            if (r.elapsed_ms < lo) lo = r.elapsed_ms;
            if (r.elapsed_ms > hi) hi = r.elapsed_ms;
        }
        vm.total_ms = sum;
        vm.avg_ms   = sum / vm.results.size();
        vm.min_ms   = lo;
        vm.max_ms   = hi;
    }

    auto summary = monitor.GetSummary();
    vm.cpu_avg_pct       = summary.cpu_avg_pct;
    vm.gpu_util_avg_pct  = summary.gpu_util_avg_pct;
    vm.gpu_util_peak_pct = summary.gpu_util_peak_pct;

    llama_sampler_free(smpl);
    llama_free(ctx);
    llama_model_free(model);

    return vm;
}

#endif // HAS_LLAMA_CPP

// ---------------------------------------------------------------------------
// Find a variant by execution-provider substring (case-insensitive)
// ---------------------------------------------------------------------------
static const ModelVariant* FindVariant(const Model& model, const std::string& epName) {
    auto toLower = [](std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    std::string target = toLower(epName);
    for (const auto& v : model.GetVariants()) {
        const auto& info = v.GetInfo();
        if (!info.runtime) continue;
        std::string ep = toLower(info.runtime->execution_provider);
        if (ep.find(target) != std::string::npos) return &v;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Run all translations on one variant with full metrics collection
// ---------------------------------------------------------------------------
static VariantMetrics RunVariant(Model& model,
                                 const ModelVariant& variant,
                                 const std::vector<TranslationItem>& items,
                                 const std::string& label) {
    VariantMetrics vm;
    vm.label = label;
    vm.group = "Foundry Local";
    const auto& info = variant.GetInfo();
    if (info.runtime) {
        vm.ep_name     = info.runtime->execution_provider;
        vm.device_type = (info.runtime->device_type == DeviceType::GPU  ? "GPU"
                         : info.runtime->device_type == DeviceType::NPU ? "NPU"
                         : "CPU");
    }
    vm.model_size_mb = info.file_size_mb;
    vm.runtime_dep_size = "/";  // filled in by caller after return

    model.SelectVariant(variant);

    std::cout << "Downloading " << label << " variant...\n";
    model.Download([&](float pct) {
        printf("\r  %s: %5.1f%%", label.c_str(), pct);
        fflush(stdout);
        return true;
    });
    std::cout << "\n";

    std::cout << "Loading " << label << " variant...\n";
    model.Load();

    if (!model.IsLoaded()) {
        std::cerr << "Failed to load " << label << " variant.\n";
        return vm;
    }

    OpenAIChatClient chat(model);

    // Warmup (stabilise CUDA graph capture / WebGPU shader compilation)
    std::cout << "  [" << label << "] warmup..." << std::flush;
    try {
        (void)TranslateOne(chat, "en->zh", "Hello");
    } catch (...) {}
    std::cout << " done\n";

    // Snapshot absolute memory after model load + warmup
    vm.rss_mb  = GetProcessRssMb();
    vm.vram_mb = NvidiaSmiQuery("memory.used");

    ResourceMonitor monitor(100);
    monitor.Start();

    // Run translations
    vm.results.reserve(items.size());
    for (const auto& item : items) {
        std::cout << "  [" << label << "] #" << item.id << " " << item.direction
                  << " \"" << item.source << "\" ... " << std::flush;
        try {
            auto res = TranslateOne(chat, item.direction, item.source);
            std::cout << res.text << "  (" << std::fixed << std::setprecision(0)
                      << res.elapsed_ms << " ms)\n";
            vm.results.push_back(std::move(res));
        } catch (const std::exception& ex) {
            std::cout << "(error: " << ex.what() << ")\n";
            vm.results.push_back({"(error)", 0.0});
        }
    }

    monitor.Stop();

    // Compute aggregate timing
    if (!vm.results.empty()) {
        double sum = 0;
        double lo  = vm.results[0].elapsed_ms;
        double hi  = vm.results[0].elapsed_ms;
        for (const auto& r : vm.results) {
            sum += r.elapsed_ms;
            if (r.elapsed_ms < lo) lo = r.elapsed_ms;
            if (r.elapsed_ms > hi) hi = r.elapsed_ms;
        }
        vm.total_ms = sum;
        vm.avg_ms   = sum / vm.results.size();
        vm.min_ms   = lo;
        vm.max_ms   = hi;
    }

    auto summary = monitor.GetSummary();
    vm.cpu_avg_pct       = summary.cpu_avg_pct;
    vm.gpu_util_avg_pct  = summary.gpu_util_avg_pct;
    vm.gpu_util_peak_pct = summary.gpu_util_peak_pct;

    model.Unload();
    return vm;
}

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------
static std::string FmtSize(std::optional<double> v) {
    if (!v) return "/";
    char b[32];
    if (*v < 1024) snprintf(b, sizeof(b), "%.0f MB", *v);
    else           snprintf(b, sizeof(b), "%.2f GB", *v / 1024.0);
    return b;
}
static std::string FmtSizeInt(std::optional<int> v) {
    if (!v) return "/";
    return FmtSize(static_cast<double>(*v));
}
static std::string FmtMs(double v) {
    char b[32]; snprintf(b, sizeof(b), "%.0f ms", v); return b;
}
static std::string FmtPct(std::optional<double> v) {
    if (!v) return "/";
    char b[32]; snprintf(b, sizeof(b), "%.1f%%", *v); return b;
}

/// Compute total size of DLLs/SOs in a directory tree (non-recursive by default).
static double DirDllSizeMb(const std::string& dir) {
    double total = 0;
    try {
        for (auto& entry : std::filesystem::directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext == ".dll" || ext == ".so" || ext == ".dylib")
                total += static_cast<double>(entry.file_size());
        }
    } catch (...) {}
    return total / (1024.0 * 1024.0);
}

// ---------------------------------------------------------------------------
// Print side-by-side comparison table (matches target format)
// ---------------------------------------------------------------------------
static void PrintComparisonTable(const std::vector<VariantMetrics>& cols) {
    if (cols.empty()) return;
    const int labelW = 36;
    const int colW   = 22;

    auto printRow = [&](const std::string& metric,
                        std::function<std::string(const VariantMetrics&)> fn) {
        printf("%-*s", labelW, metric.c_str());
        for (const auto& c : cols)
            printf("%*s", colW, fn(c).c_str());
        printf("\n");
    };

    int totalW = labelW + colW * static_cast<int>(cols.size());
    std::string sep(totalW, '-');
    std::string dsep(totalW, '=');

    // --- Group header row ---
    printf("\n%s\n", dsep.c_str());
    // Collect unique groups in order
    std::vector<std::pair<std::string, int>> groups; // (group_name, span_count)
    for (const auto& c : cols) {
        if (groups.empty() || groups.back().first != c.group)
            groups.push_back({c.group, 1});
        else
            groups.back().second++;
    }
    printf("%-*s", labelW, "");
    for (const auto& [gname, span] : groups) {
        int w = colW * span;
        printf("%*s", w, gname.c_str());
    }
    printf("\n");

    // --- Column label row ---
    printf("%-*s", labelW, "Model");
    for (const auto& c : cols) printf("%*s", colW, c.label.c_str());
    printf("\n%s\n", sep.c_str());

    // --- Metric rows ---
    printRow("Runtime dependency size", [](const VariantMetrics& m) { return m.runtime_dep_size; });
    printRow("Model size",             [](const VariantMetrics& m) { return FmtSizeInt(m.model_size_mb); });
    printRow("RAM usage",              [](const VariantMetrics& m) { return FmtSize(m.rss_mb); });
    printRow("VRAM usage",             [](const VariantMetrics& m) { return FmtSize(m.vram_mb); });
    printRow("Translation latency",    [](const VariantMetrics& m) { return FmtMs(m.avg_ms); });
    printRow("CPU usage (avg)",        [](const VariantMetrics& m) { return FmtPct(m.cpu_avg_pct); });
    printRow("GPU-3D usage (avg)",     [](const VariantMetrics& m) { return FmtPct(m.gpu_util_avg_pct); });
    printRow("GPU-3D usage (max)",     [](const VariantMetrics& m) { return FmtPct(m.gpu_util_peak_pct); });
    printf("%s\n", sep.c_str());
}

// ---------------------------------------------------------------------------
// Print per-item results + write TSV
// ---------------------------------------------------------------------------
static void PrintTranslationResults(const std::vector<TranslationItem>& items,
                                    const std::vector<VariantMetrics>& cols) {
    // Console table
    printf("\n%-4s%-8s%-40s", "#", "Dir", "Source");
    for (const auto& c : cols)
        printf("%-30s", c.label.c_str());
    printf("\n");

    for (size_t i = 0; i < items.size(); ++i) {
        printf("%-4d%-8s%-40s",
               items[i].id, items[i].direction.c_str(), items[i].source.c_str());
        for (const auto& c : cols) {
            std::string text = (i < c.results.size()) ? c.results[i].text : "(skipped)";
            if (text.size() > 28) text = text.substr(0, 25) + "...";
            printf("%-30s", text.c_str());
        }
        printf("\n");
    }

    // Write TSV file (UTF-8 with BOM)
    std::ofstream out("translation_perf_results.tsv", std::ios::binary);
    if (out) {
        out << "\xEF\xBB\xBF";
        out << "#\tDirection\tSource";
        for (const auto& c : cols)
            out << "\t" << c.label << "\t" << c.label << " (ms)";
        out << "\n";
        for (size_t i = 0; i < items.size(); ++i) {
            out << items[i].id << "\t" << items[i].direction << "\t" << items[i].source;
            for (const auto& c : cols) {
                if (i < c.results.size()) {
                    out << "\t" << c.results[i].text
                        << "\t" << std::fixed << std::setprecision(0)
                        << c.results[i].elapsed_ms;
                } else {
                    out << "\t(skipped)\t";
                }
            }
            out << "\n";
        }
        std::cout << "\nResults written to translation_perf_results.tsv\n";
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    const std::string chatAlias = (argc > 1) ? argv[1] : "qwen3.5-2b-text";
    const std::string ggufPath  = (argc > 2) ? argv[2] : "";

    // --- Translation data (same 20 items as translation-benchmark) ---
    std::vector<TranslationItem> items = {
        { 1, "zh->en", u8"你好"},
        { 2, "zh->en", u8"主播睡醒了"},
        { 3, "zh->en", u8"玩的什么游戏啊"},
        { 4, "zh->en", u8"听不见声音"},
        { 5, "zh->en", u8"主播在哪里啊"},
        { 6, "zh->en", u8"欢迎进入直播间的小伙伴"},
        { 7, "zh->en", u8"哈哈哈哈哈"},
        { 8, "zh->en", u8"我也会"},
        { 9, "zh->en", u8"我先走了 下次见"},
        {10, "zh->en", u8"这个游戏咋样"},
        {11, "en->zh", "First time here, love the stream!"},
        {12, "en->zh", "Streamer, your skills are amazing"},
        {13, "en->zh", "What's the name of this game?"},
        {14, "en->zh", "Can you turn up the mic?"},
        {15, "en->zh", "Hi from the US!"},
        {16, "en->zh", "Where can I follow you?"},
        {17, "en->zh", "Welcome to all the new viewers"},
        {18, "en->zh", "LOL that was hilarious"},
        {19, "en->zh", "I gotta go, see you tomorrow"},
        {20, "en->zh", "Is the streamer AFK?"},
    };

    try {
        QuietLogger logger;
        Manager::Create({"TranslationPerfBenchmark"}, &logger);
        auto& manager = Manager::Instance();

        // Discover and register execution providers
        try {
            auto eps = manager.DiscoverEps();
            std::cout << "Available execution providers:\n";
            for (const auto& ep : eps)
                std::cout << "  " << ep.name << "\n";

            if (!eps.empty()) {
                std::cout << "Downloading execution providers...\n";
                manager.DownloadAndRegisterEps([](const std::string& epName, double percent) {
                    printf("\r  %-30s  %5.1f%%", epName.c_str(), percent);
                    fflush(stdout);
                });
                std::cout << "\n";
            }
        } catch (const std::exception& ex) {
            std::cerr << "EP discovery skipped: " << ex.what() << "\n";
        }

        // Get the model
        auto& catalog = manager.GetCatalog();
        auto* imodel = catalog.GetModel(chatAlias);
        if (!imodel) {
            std::cerr << "Model '" << chatAlias << "' not found in catalog.\n";
            Manager::Destroy();
            return 1;
        }

        auto* model = dynamic_cast<Model*>(imodel);
        if (!model) {
            std::cerr << "Unexpected model type for '" << chatAlias << "'.\n";
            Manager::Destroy();
            return 1;
        }

        // List available variants
        std::cout << "\nVariants for '" << chatAlias << "':\n";
        for (const auto& v : model->GetVariants()) {
            const auto& info = v.GetInfo();
            std::cout << "  " << info.name;
            if (info.runtime)
                std::cout << "  [ep=" << info.runtime->execution_provider
                          << ", device=" << (info.runtime->device_type == DeviceType::GPU ? "GPU"
                                            : info.runtime->device_type == DeviceType::NPU ? "NPU"
                                            : "CPU") << "]";
            if (info.file_size_mb)
                std::cout << "  " << *info.file_size_mb << " MB";
            std::cout << "\n";
        }

        // Find CUDA, WebGPU, and CPU variants
        const ModelVariant* cudaVariant   = FindVariant(*model, "CUDA");
        const ModelVariant* webgpuVariant = FindVariant(*model, "WebGPU");
        const ModelVariant* cpuVariant    = FindVariant(*model, "CPU");

        std::vector<VariantMetrics> columns;

        // Compute Foundry Local runtime DLL size (EP-specific + FL SDK dll)
        // The DLLs are copied next to the executable by fl_copy_runtime_dlls.
        std::string flGroup = "Foundry Local " + chatAlias;
        auto computeFlRuntimeSize = [](const std::string& /*label*/) -> std::string {
            // Measure DLLs next to executable
            char exePath[MAX_PATH]{};
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
            double totalMb = DirDllSizeMb(exeDir.string());
            char buf[64];
            if (totalMb >= 1024)
                snprintf(buf, sizeof(buf), "%.2f GB", totalMb / 1024.0);
            else
                snprintf(buf, sizeof(buf), "%.0f MB", totalMb);
            return buf;
        };

        // --- CUDA ---
        if (cudaVariant) {
            std::cout << "\n=== CUDA Translations ===\n";
            auto vm = RunVariant(*model, *cudaVariant, items, "CUDA");
            vm.group = flGroup;
            vm.runtime_dep_size = computeFlRuntimeSize("CUDA");
            columns.push_back(std::move(vm));
        } else {
            std::cout << "\nCUDA variant not available — skipping.\n";
        }

        // --- WebGPU ---
        if (webgpuVariant) {
            std::cout << "\n=== WebGPU Translations ===\n";
            auto vm = RunVariant(*model, *webgpuVariant, items, "WebGPU");
            vm.group = flGroup;
            vm.runtime_dep_size = computeFlRuntimeSize("WebGPU");
            columns.push_back(std::move(vm));
        } else {
            std::cout << "\nWebGPU variant not available — skipping.\n";
        }

        // --- CPU ---
        if (cpuVariant) {
            std::cout << "\n=== CPU Translations ===\n";
            auto vm = RunVariant(*model, *cpuVariant, items, "CPU");
            vm.group = flGroup;
            vm.runtime_dep_size = computeFlRuntimeSize("CPU");
            columns.push_back(std::move(vm));
        } else {
            std::cout << "\nCPU variant not available — skipping.\n";
        }

        // --- llama.cpp GPU ---
#ifdef HAS_LLAMA_CPP
        if (!ggufPath.empty()) {
            // GPU run: load all backends (CUDA, etc.)
            llama_backend_init();
            ggml_backend_load_all();

            std::cout << "\n=== llama.cpp GPU Translations ===\n";
            columns.push_back(RunLlamaCpp(ggufPath, items, "llama.cpp GPU", -1));

            llama_backend_free();

            // CPU run: reinit backend WITHOUT loading GPU backends
            llama_backend_init();
            // Intentionally do NOT call ggml_backend_load_all() — CPU only

            std::cout << "\n=== llama.cpp CPU Translations ===\n";
            columns.push_back(RunLlamaCpp(ggufPath, items, "llama.cpp CPU", 0));

            llama_backend_free();
        }
#else
        if (!ggufPath.empty()) {
            std::cerr << "\nllama.cpp support not compiled in. "
                         "Rebuild with -DUSE_LLAMA_CPP=ON and set LLAMA_CPP_DIR.\n";
        }
#endif

        // --- Print comparison table + per-item results ---
        PrintComparisonTable(columns);
        PrintTranslationResults(items, columns);

        Manager::Destroy();
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal: " << ex.what() << std::endl;
        Manager::Destroy();
        return 1;
    }
}
