// Terminal UI helpers for Foundry Local Playground.
// Handles all terminal drawing so the main app can focus on SDK calls.

use std::io::{self, Write};

const BLOCK_FULL: char = '█';
const BLOCK_EMPTY: char = '░';
const BAR_WIDTH: usize = 30;

// ── Primitives ───────────────────────────────────────────────────────────

pub fn progress_bar(percent: f64) -> String {
    let filled = ((BAR_WIDTH as f64) * percent / 100.0) as usize;
    let empty = BAR_WIDTH.saturating_sub(filled);
    format!(
        "{}{}",
        BLOCK_FULL.to_string().repeat(filled),
        BLOCK_EMPTY.to_string().repeat(empty)
    )
}

pub fn section(title: &str) {
    let cols = term_width();
    println!();
    println!("{}", "─".repeat(cols));
    println!("  {title}");
    println!("{}", "─".repeat(cols));
}

fn term_width() -> usize {
    80 // conservative default; works cross-platform without extra deps
}

fn seg(w: usize) -> String {
    "─".repeat(w + 2)
}

fn table_hr(widths: &[usize], pos: &str) -> String {
    let (l, m, r) = match pos {
        "top" => ("┌", "┬", "┐"),
        "mid" => ("├", "┼", "┤"),
        _ => ("└", "┴", "┘"),
    };
    let segs: Vec<String> = widths.iter().map(|w| seg(*w)).collect();
    format!("  {}{}{}", l, segs.join(m), r)
}

fn pad_visible(s: &str, width: usize) -> String {
    // Strip ANSI escape sequences to compute visible length
    let visible_len = strip_ansi(s).chars().count();
    let pad = width.saturating_sub(visible_len);
    format!("{}{}", s, " ".repeat(pad))
}

fn fit_visible(s: &str, width: usize) -> String {
    // Avoid cutting ANSI sequences in status cells.
    if s.contains("\x1b[") {
        return s.to_string();
    }

    let len = s.chars().count();
    if len <= width {
        return s.to_string();
    }

    if width <= 1 {
        return s.chars().take(width).collect();
    }

    let mut out: String = s.chars().take(width - 1).collect();
    out.push('…');
    out
}

fn strip_ansi(s: &str) -> String {
    let mut result = String::new();
    let mut in_escape = false;
    for c in s.chars() {
        if in_escape {
            if c.is_ascii_alphabetic() {
                in_escape = false;
            }
        } else if c == '\x1b' {
            in_escape = true;
        } else {
            result.push(c);
        }
    }
    result
}

fn table_row(widths: &[usize], values: &[&str]) -> String {
    let mut row = String::from("  │");
    for (i, w) in widths.iter().enumerate() {
        let val = if i < values.len() { values[i] } else { "" };
        let fitted = fit_visible(val, *w);
        row.push_str(&format!(" {} │", pad_visible(&fitted, *w)));
    }
    row
}

pub fn wrap_text(text: &str, max_width: usize) -> Vec<String> {
    let mut result = Vec::new();
    for para in text.split('\n') {
        if para.is_empty() {
            result.push(String::new());
            continue;
        }
        let mut line = String::new();
        for word in para.split(' ') {
            if !line.is_empty() && line.len() + word.len() + 1 > max_width {
                result.push(line);
                line = word.to_string();
            } else if line.is_empty() {
                line = word.to_string();
            } else {
                line.push(' ');
                line.push_str(word);
            }
        }
        if !line.is_empty() {
            result.push(line);
        }
    }
    if result.is_empty() {
        result.push(String::new());
    }
    result
}

// ── EP Table ─────────────────────────────────────────────────────────────

#[derive(Clone)]
pub struct EpEntry {
    pub name: String,
    pub is_registered: bool,
}

pub fn show_ep_table(eps: &[EpEntry]) {
    if eps.is_empty() {
        println!("  No execution providers found.");
        return;
    }
    let col1 = eps.iter().map(|e| e.name.len()).max().unwrap_or(7).max(7);
    let col2 = BAR_WIDTH + 7;
    let w = &[col1, col2];

    println!("{}", table_hr(w, "top"));
    println!("{}", table_row(w, &["EP Name", "Status"]));
    println!("{}", table_hr(w, "mid"));

    for ep in eps {
        let cell = if ep.is_registered {
            "\x1b[32m● registered\x1b[0m".to_string()
        } else {
            format!("{}  0.0%", progress_bar(0.0))
        };
        println!("{}", table_row(w, &[&ep.name, &cell]));
    }
    println!("{}", table_hr(w, "bot"));
}

pub fn finalize_ep_table(eps: &[EpEntry], failed: &[String]) {
    let col1 = eps.iter().map(|e| e.name.len()).max().unwrap_or(7).max(7);
    let col2 = BAR_WIDTH + 7;
    let w = &[col1, col2];
    let total_lines = eps.len() + 4;

    print!("\x1b[{}A\r", total_lines);
    println!("{}", table_hr(w, "top"));
    println!("{}", table_row(w, &["EP Name", "Status"]));
    println!("{}", table_hr(w, "mid"));
    for ep in eps {
        let ok = ep.is_registered || !failed.contains(&ep.name);
        let dot = if ok {
            "\x1b[32m● registered\x1b[0m"
        } else {
            "\x1b[31m● failed\x1b[0m"
        };
        print!("{}\x1b[K\n", table_row(w, &[&ep.name, dot]));
    }
    print!("{}\x1b[K\n", table_hr(w, "bot"));
    io::stdout().flush().ok();
}

pub fn ep_progress_callback(eps: Vec<EpEntry>) -> impl FnMut(&str, f64) + Send + 'static {
    let col1 = eps.iter().map(|e| e.name.len()).max().unwrap_or(7).max(7);
    let col2 = BAR_WIDTH + 7;
    let widths = [col1, col2];

    move |ep_name: &str, percent: f64| {
        let idx = eps.iter().position(|e| e.name == ep_name);
        if let Some(idx) = idx {
            // Cursor starts one line below table; row 0 is `eps.len() + 1` lines up.
            let up = eps.len() - idx + 1;
            let bar = format!("{} {:5.1}%", progress_bar(percent), percent);
            let row = table_row(&widths, &[ep_name, &bar]);
            print!("\x1b[{}A\r{}\x1b[K\x1b[{}B\r", up, row, up);
            io::stdout().flush().ok();
        }
    }
}

// ── Model Catalog ────────────────────────────────────────────────────────

pub struct CatalogRow {
    pub num: usize,
    pub alias: String,
    pub variant_id: String,
    pub size_gb: String,
    pub task: String,
    pub is_cached: bool,
    pub is_first_variant: bool,
    pub model_idx: usize,
}

pub fn show_catalog(rows: &[CatalogRow]) {
    let mc_num = rows.len().to_string().len().max(2);
    let mc_alias = rows
        .iter()
        .map(|r| r.alias.len())
        .max()
        .unwrap_or(5)
        .max(5)
        .min(24);
    let mc_variant = rows
        .iter()
        .map(|r| r.variant_id.len())
        .max()
        .unwrap_or(7)
        .max(7)
        .min(50);
    let mc_size = 10;
    let mc_task = rows
        .iter()
        .map(|r| r.task.len())
        .max()
        .unwrap_or(4)
        .max(4)
        .min(28);
    let mc_cached = 6;
    let mc = &[mc_num, mc_alias, mc_variant, mc_size, mc_task, mc_cached];

    println!("{}", table_hr(mc, "top"));
    println!(
        "{}",
        table_row(
            mc,
            &["#", "Alias", "Variant", "Size (GB)", "Task", "Cached"]
        )
    );
    println!("{}", table_hr(mc, "mid"));

    let mut prev_model_idx: Option<usize> = None;
    for r in rows {
        if let Some(prev) = prev_model_idx {
            if r.model_idx != prev {
                println!("{}", table_hr(mc, "mid"));
            }
        }
        prev_model_idx = Some(r.model_idx);

        let dot = if r.is_cached {
            "\x1b[32m●\x1b[0m"
        } else {
            "\x1b[31m●\x1b[0m"
        };
        let num_str = r.num.to_string();
        println!(
            "{}",
            table_row(
                mc,
                &[
                    &num_str,
                    if r.is_first_variant { &r.alias } else { "" },
                    &r.variant_id,
                    if r.is_first_variant { &r.size_gb } else { "" },
                    if r.is_first_variant { &r.task } else { "" },
                    dot,
                ]
            )
        );
    }
    println!("{}", table_hr(mc, "bot"));
}

// ── Download Progress ────────────────────────────────────────────────────

pub fn show_download_bar(model_alias: &str) {
    let col1 = model_alias.len().max(5);
    let col2 = BAR_WIDTH + 7;
    let w = &[col1, col2];
    let fmt = |n: &str, c: &str| table_row(w, &[n, c]);

    println!("{}", table_hr(w, "top"));
    println!("{}", fmt("Model", "Progress"));
    println!("{}", table_hr(w, "mid"));
    println!(
        "{}",
        fmt(model_alias, &format!("{}  0.0%", progress_bar(0.0)))
    );
    println!("{}", table_hr(w, "bot"));
}

pub fn update_download_bar(model_alias: &str, progress_text: &str) {
    let col1 = model_alias.len().max(5);
    let col2 = BAR_WIDTH + 7;
    let w = &[col1, col2];

    // Try to parse the progress string as a percentage and render a bar.
    let display = if let Ok(percent) = progress_text.trim().parse::<f64>() {
        format!("{} {:5.1}%", progress_bar(percent), percent)
    } else {
        progress_text.to_string()
    };

    print!("\x1b[2A\r");
    print!("{}", table_row(w, &[model_alias, &display]));
    print!("\x1b[K\n\x1b[1B");
    io::stdout().flush().ok();
}

pub fn finalize_download_bar(model_alias: &str) {
    let done = format!(
        "\x1b[32m{} done \x1b[0m",
        BLOCK_FULL.to_string().repeat(BAR_WIDTH)
    );
    update_download_bar(model_alias, &done);
}

// ── Chat / Audio Streaming ───────────────────────────────────────────────

pub fn print_user_msg(text: &str) {
    let cols = term_width();
    let lines = wrap_text(text, cols.min(68).saturating_sub(8));
    for line in &lines {
        println!("  {line}");
    }
    println!();
}

pub struct StreamBox {
    box_width: usize,
    current_line: String,
    word_buf: String,
}

impl StreamBox {
    pub fn new() -> Self {
        let cols = term_width();
        let box_width = cols.min(68).saturating_sub(8);
        println!("  ┌{}┐", "─".repeat(box_width + 2));
        let row = Self::draw_row_static("", true, box_width);
        print!("{row}");
        io::stdout().flush().ok();
        Self {
            box_width,
            current_line: String::new(),
            word_buf: String::new(),
        }
    }

    fn draw_row_static(text: &str, cursor: bool, box_width: usize) -> String {
        let display = if cursor {
            format!("{text}▍")
        } else {
            text.to_string()
        };
        let pad = box_width.saturating_sub(display.chars().count());
        format!("  │ {}{} │", display, " ".repeat(pad))
    }

    fn draw_row(&self, text: &str, cursor: bool) -> String {
        Self::draw_row_static(text, cursor, self.box_width)
    }

    fn flush_line(&mut self) {
        let row = self.draw_row(&self.current_line, false);
        print!("\r{row}\x1b[K\n");
        self.current_line.clear();
    }

    fn push_word(&mut self, word: &str) {
        if self.current_line.is_empty() {
            self.current_line = word.to_string();
        } else if self.current_line.len() + 1 + word.len() <= self.box_width {
            self.current_line.push(' ');
            self.current_line.push_str(word);
        } else {
            self.flush_line();
            self.current_line = word.to_string();
        }
        while self.current_line.len() > self.box_width {
            let chunk: String = self.current_line.chars().take(self.box_width).collect();
            let rest: String = self.current_line.chars().skip(self.box_width).collect();
            let row = self.draw_row(&chunk, false);
            print!("\r{row}\x1b[K\n");
            self.current_line = rest;
        }
    }

    pub fn write_char(&mut self, c: char) {
        if c == '\n' {
            if !self.word_buf.is_empty() {
                let w = std::mem::take(&mut self.word_buf);
                self.push_word(&w);
            }
            self.flush_line();
            let row = self.draw_row("", true);
            print!("\r{row}\x1b[K");
            io::stdout().flush().ok();
        } else if c == ' ' {
            if !self.word_buf.is_empty() {
                let w = std::mem::take(&mut self.word_buf);
                self.push_word(&w);
            }
            let row = self.draw_row(&self.current_line, true);
            print!("\r{row}\x1b[K");
            io::stdout().flush().ok();
        } else {
            self.word_buf.push(c);
            let preview = if self.current_line.is_empty() {
                self.word_buf.clone()
            } else {
                format!("{} {}", self.current_line, self.word_buf)
            };
            let row = self.draw_row(&preview, true);
            print!("\r{row}\x1b[K");
            io::stdout().flush().ok();
        }
    }

    pub fn finish(&mut self) {
        if !self.word_buf.is_empty() {
            let w = std::mem::take(&mut self.word_buf);
            self.push_word(&w);
        }
        let row = self.draw_row(&self.current_line, false);
        print!("\r{row}\x1b[K\n");
        println!("  └{}┘\n", "─".repeat(self.box_width + 2));
    }
}

pub fn ask_user(prompt: &str) -> Option<String> {
    print!("{prompt}");
    io::stdout().flush().ok();
    let mut input = String::new();
    match io::stdin().read_line(&mut input) {
        Ok(0) => None,
        Ok(_) => Some(input.trim().to_string()),
        Err(_) => None,
    }
}
