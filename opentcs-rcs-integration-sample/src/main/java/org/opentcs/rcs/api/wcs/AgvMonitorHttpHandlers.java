// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import io.javalin.http.Handler;

/**
 * Minimal browser monitor for AGV missions and command outbox state.
 */
public final class AgvMonitorHttpHandlers {

  private AgvMonitorHttpHandlers() {
  }

  public static Handler pageHandler() {
    return ctx -> ctx.contentType("text/html; charset=utf-8").result(PAGE);
  }

  private static final String PAGE = """
      <!doctype html>
      <html lang="zh-CN">
      <head>
        <meta charset="utf-8"/>
        <meta name="viewport" content="width=device-width, initial-scale=1"/>
        <title>AGV Mission Monitor</title>
        <style>
          :root {
            --bg: #eef0e8;
            --paper: #fffdf5;
            --ink: #17211f;
            --muted: #66746f;
            --line: #d7d0c2;
            --ok: #21764f;
            --wait: #a5651b;
            --bad: #ad362f;
            --accent: #244f46;
            --gold: #d59d42;
          }
          * { box-sizing: border-box; }
          body {
            margin: 0;
            min-height: 100vh;
            color: var(--ink);
            font-family: "Source Han Serif SC", "Noto Serif CJK SC", "Microsoft YaHei", serif;
            background:
              radial-gradient(900px 420px at 0% -10%, rgba(213,157,66,.28), transparent 65%),
              radial-gradient(760px 420px at 100% 0%, rgba(36,79,70,.22), transparent 62%),
              linear-gradient(180deg, #f6f1e4, var(--bg));
          }
          .wrap { max-width: 1220px; margin: 0 auto; padding: 24px; }
          header { display: flex; align-items: flex-end; justify-content: space-between; gap: 12px; margin-bottom: 18px; }
          h1 { margin: 0; font-size: clamp(28px, 4vw, 48px); letter-spacing: -1px; }
          .subtitle { color: var(--muted); margin-top: 6px; font-size: 14px; }
          .pill { border: 1px solid var(--line); border-radius: 999px; padding: 8px 12px; background: rgba(255,253,245,.72); color: var(--muted); font-size: 13px; }
          .grid { display: grid; grid-template-columns: 360px 1fr; gap: 16px; align-items: start; }
          .panel { background: rgba(255,253,245,.9); border: 1px solid var(--line); border-radius: 18px; box-shadow: 0 18px 50px rgba(32,43,38,.08); overflow: hidden; }
          .panel h2 { margin: 0; padding: 14px 16px; border-bottom: 1px solid var(--line); font-size: 18px; background: rgba(255,255,255,.42); }
          .body { padding: 16px; }
          label { display: block; font-size: 12px; color: var(--muted); margin-bottom: 5px; }
          input { width: 100%; border: 1px solid var(--line); border-radius: 10px; padding: 10px 11px; background: #fffefa; color: var(--ink); font: inherit; outline: none; }
          input:focus { border-color: var(--accent); box-shadow: 0 0 0 3px rgba(36,79,70,.12); }
          .fields { display: grid; gap: 10px; }
          .two { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
          .actions { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 14px; }
          button { border: 0; border-radius: 999px; padding: 10px 14px; color: white; background: var(--accent); font-weight: 800; cursor: pointer; }
          button.secondary { background: #6a716d; }
          button.gold { background: var(--gold); color: #251b0d; }
          button:disabled { opacity: .55; cursor: wait; }
          .cards { display: grid; grid-template-columns: repeat(3, minmax(0,1fr)); gap: 12px; margin-bottom: 16px; }
          .metric { padding: 14px; border: 1px solid var(--line); border-radius: 16px; background: rgba(255,253,245,.82); min-height: 92px; }
          .metric .name { color: var(--muted); font-size: 12px; }
          .metric .value { margin-top: 9px; font-size: 24px; font-weight: 900; word-break: break-word; }
          .status { display: inline-flex; align-items: center; border-radius: 999px; padding: 4px 10px; font-size: 12px; font-weight: 900; color: white; background: #65736e; }
          .status.SUCCESS, .status.RECEIVED { background: var(--ok); }
          .status.PENDING { background: var(--wait); }
          .status.FAILED { background: var(--bad); }
          table { width: 100%; border-collapse: collapse; overflow: hidden; border-radius: 14px; }
          th, td { border-bottom: 1px solid var(--line); padding: 10px; text-align: left; vertical-align: top; font-size: 13px; }
          th { color: var(--muted); font-size: 12px; background: rgba(255,255,255,.48); }
          code { font-family: "Cascadia Mono", Consolas, monospace; font-size: 12px; word-break: break-all; }
          pre { margin: 0; padding: 14px; border-radius: 16px; background: #14201d; color: #dbe9dc; min-height: 160px; max-height: 360px; overflow: auto; font-size: 12px; line-height: 1.5; }
          .monitor-head { display: flex; align-items: center; justify-content: space-between; gap: 10px; margin-bottom: 12px; }
          .auto { display: inline-flex; align-items: center; gap: 7px; color: var(--muted); font-size: 13px; }
          .auto input { width: auto; }
          .empty { padding: 24px; text-align: center; color: var(--muted); border: 1px dashed var(--line); border-radius: 16px; background: rgba(255,253,245,.64); }
          @media (max-width: 900px) { .grid { grid-template-columns: 1fr; } .cards { grid-template-columns: 1fr; } header { align-items: flex-start; flex-direction: column; } }
        </style>
      </head>
      <body>
        <div class="wrap">
          <header>
            <div>
              <h1>AGV Mission Monitor</h1>
              <div class="subtitle">最小监控视图：创建 mission、查看 RCS 状态、查看 AGV command outbox。</div>
            </div>
            <div class="pill">RCS local page · /demo/agv-monitor</div>
          </header>

          <div class="grid">
            <section class="panel">
              <h2>任务输入</h2>
              <div class="body">
                <div class="fields">
                  <div><label>mission_no</label><input id="missionNo" value="M-MONITOR-001"/></div>
                  <div><label>task_no</label><input id="taskNo" value="T-MONITOR-001"/></div>
                  <div class="two">
                    <div><label>from_point</label><input id="fromPoint" value="Point-01"/></div>
                    <div><label>to_point</label><input id="toPoint" value="Point-02"/></div>
                  </div>
                  <div class="two">
                    <div><label>pallet_no</label><input id="palletNo" value="PLT-MONITOR-001"/></div>
                    <div><label>priority</label><input id="priority" value="50"/></div>
                  </div>
                  <div><label>callback_url</label><input id="callbackUrl" value="/demo/wcs/callback"/></div>
                </div>
                <div class="actions">
                  <button onclick="createMission()">创建 mission</button>
                  <button class="secondary" onclick="refreshAll()">刷新</button>
                  <button class="gold" onclick="fillTimestamp()">换一组编号</button>
                </div>
              </div>
            </section>

            <main>
              <div class="cards">
                <div class="metric"><div class="name">mission</div><div class="value" id="missionLabel">-</div></div>
                <div class="metric"><div class="name">RCS 状态</div><div class="value" id="missionStatus">-</div></div>
                <div class="metric"><div class="name">command 状态</div><div class="value" id="commandSummary">-</div></div>
              </div>

              <section class="panel">
                <h2>AGV command outbox</h2>
                <div class="body">
                  <div class="monitor-head">
                    <div class="subtitle">查询接口：/api/v1/wcs/agv/missions/{mission_no}/commands</div>
                    <label class="auto"><input type="checkbox" id="autoRefresh" onchange="toggleAuto()"/> 自动刷新</label>
                  </div>
                  <div id="commandsTable" class="empty">还没有查询结果</div>
                </div>
              </section>

              <section class="panel" style="margin-top:16px;">
                <h2>原始响应</h2>
                <div class="body"><pre id="raw">等待操作...</pre></div>
              </section>
            </main>
          </div>
        </div>

        <script>
          let timer = null;
          const $ = id => document.getElementById(id);
          const asJson = value => JSON.stringify(value, null, 2);
          const status = value => value ? `<span class="status ${value}">${value}</span>` : '-';

          function payload() {
            return {
              mission_no: $('missionNo').value.trim(),
              task_no: $('taskNo').value.trim(),
              from_point: $('fromPoint').value.trim(),
              to_point: $('toPoint').value.trim(),
              pallet_no: $('palletNo').value.trim(),
              priority: Number($('priority').value || 50),
              callback_url: $('callbackUrl').value.trim()
            };
          }

          async function request(url, options = {}) {
            const res = await fetch(url, options);
            const text = await res.text();
            let body;
            try { body = text ? JSON.parse(text) : null; }
            catch (_) { body = text; }
            if (!res.ok) throw { status: res.status, body };
            return body;
          }

          async function createMission() {
            try {
              const body = await request('/api/v1/wcs/agv/missions', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload())
              });
              $('raw').textContent = asJson(body);
              await refreshAll();
            }
            catch (err) {
              $('raw').textContent = asJson(err);
            }
          }

          async function refreshMission() {
            const missionNo = $('missionNo').value.trim();
            if (!missionNo) return null;
            const body = await request(`/api/v1/wcs/agv/missions/${encodeURIComponent(missionNo)}`);
            $('missionLabel').textContent = missionNo;
            $('missionStatus').innerHTML = status(body.data && body.data.rcs_status);
            return body;
          }

          async function refreshCommands() {
            const missionNo = $('missionNo').value.trim();
            if (!missionNo) return null;
            const body = await request(`/api/v1/wcs/agv/missions/${encodeURIComponent(missionNo)}/commands`);
            renderCommands((body && body.data) || []);
            return body;
          }

          async function refreshAll() {
            try {
              const mission = await refreshMission().catch(err => err);
              const commands = await refreshCommands().catch(err => err);
              $('raw').textContent = asJson({ mission, commands });
            }
            catch (err) {
              $('raw').textContent = asJson(err);
            }
          }

          function renderCommands(commands) {
            if (!commands.length) {
              $('commandSummary').textContent = '0';
              $('commandsTable').className = 'empty';
              $('commandsTable').innerHTML = '没有 command outbox 记录';
              return;
            }
            const counts = commands.reduce((acc, item) => {
              acc[item.status] = (acc[item.status] || 0) + 1;
              return acc;
            }, {});
            $('commandSummary').innerHTML = Object.entries(counts)
              .map(([key, val]) => `${status(key)} × ${val}`)
              .join(' ');
            $('commandsTable').className = '';
            $('commandsTable').innerHTML = `
              <table>
                <thead><tr><th>stage</th><th>status</th><th>retry</th><th>nextRetryAt</th><th>payload</th><th>lastError</th></tr></thead>
                <tbody>${commands.map(item => `
                  <tr>
                    <td><code>${escapeHtml(item.commandStage || '')}</code></td>
                    <td>${status(item.status)}</td>
                    <td>${item.retryCount ?? 0}</td>
                    <td><code>${escapeHtml(String(item.nextRetryAt ?? ''))}</code></td>
                    <td><code>${escapeHtml(item.payloadJson || '')}</code></td>
                    <td><code>${escapeHtml(item.lastError || '')}</code></td>
                  </tr>
                `).join('')}</tbody>
              </table>`;
          }

          function toggleAuto() {
            if ($('autoRefresh').checked) {
              refreshAll();
              timer = setInterval(refreshAll, 1500);
            }
            else if (timer) {
              clearInterval(timer);
              timer = null;
            }
          }

          function fillTimestamp() {
            const stamp = new Date().toISOString().replace(/[-:.TZ]/g, '').slice(0, 14);
            $('missionNo').value = `M-MON-${stamp}`;
            $('taskNo').value = `T-MON-${stamp}`;
            $('palletNo').value = `PLT-${stamp}`;
          }

          function escapeHtml(value) {
            return String(value).replace(/[&<>'"]/g, char => ({
              '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;'
            }[char]));
          }
        </script>
      </body>
      </html>
      """;
}
