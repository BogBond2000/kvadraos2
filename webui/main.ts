interface Stats {
    timestamp: number;
    cpu_total: number;
    num_cpus: number;
    cpu_per_core: number[];
    memory: { total_kb: number; used_kb: number; free_kb: number; available_kb: number };
    swap: { total_kb: number; used_kb: number };
    load_avg: [number, number, number];
    top_procs: Array<{ pid: number; name: string; cpu_usage: number; memory_rss_kb: number }>;
}

async function fetchStats(): Promise<Stats | null> {
    try {
        const response = await fetch('/api/stats');
        if (!response.ok) return null;
        return await response.json();
    } catch (e) {
        console.error(e);
        return null;
    }
}

function updateUI(data: Stats) {
    // Timestamp
    const ts = new Date(data.timestamp * 1000).toLocaleTimeString();
    document.getElementById('timestamp')!.textContent = ts;

    // Total CPU
    const cpuTotal = data.cpu_total.toFixed(1);
    document.getElementById('cpu-total')!.textContent = cpuTotal + '%';
    (document.getElementById('cpu-fill') as HTMLElement).style.width = cpuTotal + '%';

    // Memory
    const memPercent = (data.memory.used_kb / data.memory.total_kb) * 100;
    document.getElementById('mem-value')!.textContent = memPercent.toFixed(1) + '%';
    (document.getElementById('mem-fill') as HTMLElement).style.width = memPercent + '%';

    // Swap
    let swapPercent = 0;
    if (data.swap.total_kb > 0) {
        swapPercent = (data.swap.used_kb / data.swap.total_kb) * 100;
    }
    document.getElementById('swap-value')!.textContent = swapPercent.toFixed(1) + '%';
    (document.getElementById('swap-fill') as HTMLElement).style.width = swapPercent + '%';

    // Load average
    document.getElementById('load1')!.textContent = data.load_avg[0].toFixed(2);
    document.getElementById('load5')!.textContent = data.load_avg[1].toFixed(2);
    document.getElementById('load15')!.textContent = data.load_avg[2].toFixed(2);

    // Per-core
    const coresDiv = document.getElementById('cores')!;
    coresDiv.innerHTML = '';
    for (let i = 0; i < data.num_cpus; i++) {
        const usage = data.cpu_per_core[i] || 0;
        const coreDiv = document.createElement('div');
        coreDiv.className = 'core-item';
        coreDiv.innerHTML = `
            <div>CPU${i}</div>
            <div>${usage.toFixed(1)}%</div>
            <div class="core-bar"><div class="core-fill" style="width:${usage}%"></div></div>
        `;
        coresDiv.appendChild(coreDiv);
    }

    // Process table
    const tbody = document.getElementById('proc-table') as HTMLTableSectionElement;
    tbody.innerHTML = '';
    for (const proc of data.top_procs) {
        const row = tbody.insertRow();
        row.insertCell(0).textContent = proc.pid.toString();
        row.insertCell(1).textContent = proc.name;
        row.insertCell(2).textContent = proc.cpu_usage.toFixed(1) + '%';
        row.insertCell(3).textContent = proc.memory_rss_kb.toLocaleString();
    }
}

async function refresh() {
    const stats = await fetchStats();
    if (stats) updateUI(stats);
}

setInterval(refresh, 1000);
refresh();
