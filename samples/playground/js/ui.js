// Terminal UI helpers for Foundry Local Playground
// Handles all terminal drawing so the main app can focus on SDK calls.

const BLOCK_FULL = '█';
const BLOCK_EMPTY = '░';
const BAR_WIDTH = 30;

// ── Primitives ───────────────────────────────────────────────────────────

export function progressBar(percent, width = BAR_WIDTH) {
    const filled = Math.floor(width * percent / 100);
    return BLOCK_FULL.repeat(filled) + BLOCK_EMPTY.repeat(width - filled);
}

export function section(title) {
    const cols = process.stdout.columns || 80;
    console.log(`\n${'─'.repeat(cols)}`);
    console.log(`  ${title}`);
    console.log('─'.repeat(cols));
}

function padVisible(str, width) {
    const visible = str.replace(/\x1b\[[0-9;]*m/g, '');
    return str + ' '.repeat(Math.max(0, width - visible.length));
}

function seg(w) { return '─'.repeat(w + 2); }

function tableHr(widths, pos) {
    const [L, M, R] = pos === 'top' ? ['┌', '┬', '┐']
                     : pos === 'mid' ? ['├', '┼', '┤']
                                     : ['└', '┴', '┘'];
    return '  ' + L + widths.map(w => seg(w)).join(M) + R;
}

function tableRow(widths, values) {
    return '  │' + widths.map((w, i) =>
        ` ${padVisible(values[i] || '', w)} │`
    ).join('');
}

export function wrapText(text, maxWidth) {
    const result = [];
    for (const paragraph of text.split('\n')) {
        if (!paragraph.length) { result.push(''); continue; }
        let line = '';
        for (const word of paragraph.split(' ')) {
            if (line.length + word.length + (line ? 1 : 0) > maxWidth && line.length) {
                result.push(line); line = word;
            } else {
                line = line ? `${line} ${word}` : word;
            }
        }
        if (line.length) result.push(line);
    }
    return result.length ? result : [''];
}

// ── EP Table with live progress ──────────────────────────────────────────

export function showEpTable(eps) {
    if (!eps.length) { console.log('  No execution providers found.'); return { onProgress() {}, finalize() {} }; }

    const COL1 = Math.max(7, ...eps.map(ep => ep.name.length));
    const COL2 = BAR_WIDTH + 7;
    const W = [COL1, COL2];
    const fmt = (name, cell) => tableRow(W, [name, cell]);

    console.log(tableHr(W, 'top'));
    console.log(fmt('EP Name', 'Status'));
    console.log(tableHr(W, 'mid'));

    const epIdx = new Map(eps.map((ep, i) => [ep.name, i]));
    for (const ep of eps) {
        console.log(fmt(ep.name,
            ep.isRegistered ? '\x1b[32m● registered\x1b[0m'
                            : progressBar(0) + '  0.0%'));
    }
    console.log(tableHr(W, 'bot'));

    const onProgress = (epName, percent) => {
        const idx = epIdx.get(epName);
        if (idx === undefined) return;
        const up = (eps.length - idx - 1) + 1;
        process.stdout.write(`\x1b[${up}A\r`);
        process.stdout.write(fmt(epName, progressBar(percent) + ` ${percent.toFixed(1).padStart(5)}%`));
        process.stdout.write(`\x1b[K\n\x1b[${up - 1}B`);
    };

    const finalize = (result) => {
        const failedSet = new Set(result?.failedEps || []);
        const totalLines = eps.length + 4;
        process.stdout.write(`\x1b[${totalLines}A\r`);
        console.log(tableHr(W, 'top'));
        console.log(fmt('EP Name', 'Status'));
        console.log(tableHr(W, 'mid'));
        for (const ep of eps) {
            const ok = ep.isRegistered || !failedSet.has(ep.name);
            const dot = ok ? '\x1b[32m● registered\x1b[0m' : '\x1b[31m● failed\x1b[0m';
            process.stdout.write(fmt(ep.name, dot) + '\x1b[K\n');
        }
        process.stdout.write(tableHr(W, 'bot') + '\x1b[K\n');
    };

    return { onProgress, finalize };
}

// ── Model Catalog Table ──────────────────────────────────────────────────

export function showCatalog(models) {
    const rows = [];
    for (let i = 0; i < models.length; i++) {
        const m = models[i];
        for (let v = 0; v < m.variants.length; v++) {
            rows.push({ modelIdx: i, variantIdx: v, model: m, variant: m.variants[v] });
        }
    }

    const MC = [
        Math.max(2, String(rows.length).length),
        Math.max(5, ...models.map(m => m.alias.length)),
        Math.max(7, ...rows.map(r => r.variant.id.length)),
        10,
        Math.max(4, ...models.map(m => (m.info?.task || '?').length)),
        6,
    ];

    console.log(tableHr(MC, 'top'));
    console.log(tableRow(MC, ['#', 'Alias', 'Variant', 'Size (GB)', 'Task', 'Cached']));
    console.log(tableHr(MC, 'mid'));

    let num = 1;
    for (let i = 0; i < models.length; i++) {
        if (i > 0) console.log(tableHr(MC, 'mid'));
        const m = models[i];
        const sizeGb = m.info?.fileSizeMb ? (m.info.fileSizeMb / 1024).toFixed(1) : '?';
        const task = m.info?.task || '?';
        for (let v = 0; v < m.variants.length; v++) {
            const dot = m.variants[v].isCached ? '\x1b[32m●\x1b[0m' : '\x1b[31m●\x1b[0m';
            console.log(tableRow(MC, [
                String(num++),
                v === 0 ? m.alias : '',
                m.variants[v].id,
                v === 0 ? sizeGb : '',
                v === 0 ? task : '',
                dot,
            ]));
        }
    }
    console.log(tableHr(MC, 'bot'));
    return rows;
}

// ── Download Progress Bar ────────────────────────────────────────────────

export function createDownloadBar(modelAlias) {
    const COL1 = Math.max(5, modelAlias.length);
    const COL2 = BAR_WIDTH + 7;
    const W = [COL1, COL2];
    const fmt = (n, c) => tableRow(W, [n, c]);

    console.log(tableHr(W, 'top'));
    console.log(fmt('Model', 'Progress'));
    console.log(tableHr(W, 'mid'));
    console.log(fmt(modelAlias, progressBar(0) + '  0.0%'));
    console.log(tableHr(W, 'bot'));

    const onProgress = (percent) => {
        process.stdout.write('\x1b[2A\r');
        process.stdout.write(fmt(modelAlias, progressBar(percent) + ` ${percent.toFixed(1).padStart(5)}%`));
        process.stdout.write('\x1b[K\n\x1b[1B');
    };

    const finalize = () => {
        process.stdout.write('\x1b[2A\r');
        process.stdout.write(fmt(modelAlias, '\x1b[32m' + BLOCK_FULL.repeat(BAR_WIDTH) + ' done \x1b[0m'));
        process.stdout.write('\x1b[K\n\x1b[1B');
    };

    return { onProgress, finalize };
}

// ── Chat / Audio Streaming UI ────────────────────────────────────────────

export function printUserMsg(text) {
    const cols = process.stdout.columns || 80;
    const lines = wrapText(text, Math.min(cols - 8, 60));
    for (const line of lines) console.log(`  ${line}`);
    console.log();
}

export function createStreamBox() {
    const cols = process.stdout.columns || 80;
    const boxWidth = Math.min(cols - 8, 60);
    const drawRow = (text, cursor = false) => {
        const display = cursor ? text + '▍' : text;
        return `  │ ${display.padEnd(boxWidth)} │`;
    };

    console.log(`  ┌${'─'.repeat(boxWidth + 2)}┐`);
    process.stdout.write(drawRow('', true));
    let currentLine = '';
    let wordBuf = '';

    function flushLine() {
        process.stdout.write(`\r${drawRow(currentLine)}\x1b[K\n`);
        currentLine = '';
    }

    function pushWord(word) {
        if (!currentLine) { currentLine = word; }
        else if (currentLine.length + 1 + word.length <= boxWidth) { currentLine += ' ' + word; }
        else { flushLine(); currentLine = word; }
        while (currentLine.length > boxWidth) {
            process.stdout.write(`\r${drawRow(currentLine.slice(0, boxWidth))}\x1b[K\n`);
            currentLine = currentLine.slice(boxWidth);
        }
    }

    return {
        write(char) {
            if (char === '\n') {
                if (wordBuf) { pushWord(wordBuf); wordBuf = ''; }
                flushLine();
                process.stdout.write(`\r${drawRow('', true)}\x1b[K`);
            } else if (char === ' ') {
                if (wordBuf) { pushWord(wordBuf); wordBuf = ''; }
                process.stdout.write(`\r${drawRow(currentLine, true)}\x1b[K`);
            } else {
                wordBuf += char;
                const preview = currentLine ? currentLine + ' ' + wordBuf : wordBuf;
                process.stdout.write(`\r${drawRow(preview, true)}\x1b[K`);
            }
        },
        finish() {
            if (wordBuf) { pushWord(wordBuf); wordBuf = ''; }
            process.stdout.write(`\r${drawRow(currentLine)}\x1b[K\n`);
            console.log(`  └${'─'.repeat(boxWidth + 2)}┘\n`);
        },
    };
}

// ── User Input ───────────────────────────────────────────────────────────

export function askUser(rl, prompt = '  \x1b[36m> \x1b[0m') {
    return new Promise(resolve => rl.question(prompt, resolve));
}
