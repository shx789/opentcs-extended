// SPDX-FileCopyrightText: The openTCS Authors
// SPDX-License-Identifier: MIT
package org.opentcs.rcs.api.wcs;

import io.javalin.http.Context;
import io.javalin.http.Handler;
import java.time.Instant;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import org.opentcs.rcs.api.dto.ApiResponse;

/**
 * Lightweight browser demo endpoints for simulating WCS operations.
 */
public final class WcsDemoHttpHandlers {

  private WcsDemoHttpHandlers() {
  }

  public static Handler pageHandler(boolean openTcsHttpEnabled, String defaultOpenTcsBaseUrl) {
    return ctx -> ctx.contentType("text/html; charset=utf-8")
        .result(buildPage(openTcsHttpEnabled, defaultOpenTcsBaseUrl));
  }

  public static Handler callbackReceiverHandler(List<DemoCallbackRecord> callbackRecords) {
    Objects.requireNonNull(callbackRecords, "callbackRecords");
    return ctx -> {
      String payload = ctx.body();
      DemoCallbackRecord record = new DemoCallbackRecord(
          Instant.now().toString(),
          payload,
          trimHeader(ctx, "X-Trace-Id"),
          trimHeader(ctx, "X-Request-Id")
      );
      synchronized (callbackRecords) {
        callbackRecords.add(record);
        if (callbackRecords.size() > 200) {
          callbackRecords.remove(0);
        }
      }
      ctx.status(200).json(
          ApiResponse.success(
              Map.of(
                  "wcs_status",
                  "ACCEPTED",
                  "stored",
                  true
              )
          )
      );
    };
  }

  public static Handler listCallbacksHandler(List<DemoCallbackRecord> callbackRecords) {
    Objects.requireNonNull(callbackRecords, "callbackRecords");
    return ctx -> {
      List<DemoCallbackRecord> snapshot;
      synchronized (callbackRecords) {
        snapshot = new ArrayList<>(callbackRecords);
      }
      ctx.status(200).json(ApiResponse.success(snapshot));
    };
  }

  public static Handler clearCallbacksHandler(List<DemoCallbackRecord> callbackRecords) {
    Objects.requireNonNull(callbackRecords, "callbackRecords");
    return ctx -> {
      synchronized (callbackRecords) {
        callbackRecords.clear();
      }
      ctx.status(200).json(ApiResponse.success(Map.of("cleared", true)));
    };
  }

  private static String trimHeader(Context ctx, String headerName) {
    String value = ctx.header(headerName);
    if (value == null || value.isBlank()) {
      return null;
    }
    return value.trim();
  }

  private static String buildPage(boolean openTcsHttpEnabled, String defaultOpenTcsBaseUrl) {
    String mode = openTcsHttpEnabled ? "LIVE openTCS HTTP mode" : "SIMULATION mode (in-memory order client)";
    String openTcsBaseUrlValue = defaultOpenTcsBaseUrl == null || defaultOpenTcsBaseUrl.isBlank()
        ? "http://127.0.0.1:55200"
        : defaultOpenTcsBaseUrl.trim();
    String modeLabel = escapeHtml(mode);
    String openTcsBaseUrlJs = escapeJsSingleQuoted(openTcsBaseUrlValue);
    return """
        <!doctype html>
        <html lang="zh-CN">
        <head>
          <meta charset="utf-8"/>
          <meta name="viewport" content="width=device-width, initial-scale=1"/>
          <title>RCS/WCS 联调控制台</title>
          <style>
            :root {
              --bg: #f5f1e8;
              --panel: #fffdf8;
              --line: #d9cdbd;
              --ink: #1f2d2c;
              --sub: #60726f;
              --accent: #0d7a6b;
              --accent-2: #d26f37;
              --danger: #b93f33;
              --ok: #2a8a46;
            }
            * { box-sizing: border-box; }
            html {
              scrollbar-gutter: stable;
            }
            body {
              margin: 0;
              color: var(--ink);
              font-family: "Source Han Sans SC", "PingFang SC", "Microsoft YaHei UI", sans-serif;
              background:
                radial-gradient(1200px 500px at 5% -10%, #f0d8b5 0%, rgba(240,216,181,0) 70%),
                radial-gradient(900px 500px at 90% -20%, #c4e1d7 0%, rgba(196,225,215,0) 70%),
                var(--bg);
              min-height: 100vh;
              overflow-y: scroll;
            }
            .page { max-width: 1320px; margin: 0 auto; padding: 22px; }
            .top {
              margin-bottom: 14px;
              display: flex;
              flex-wrap: wrap;
              align-items: center;
              justify-content: space-between;
              gap: 10px;
            }
            h1 { margin: 0; font-size: 28px; letter-spacing: .3px; }
            .mode {
              padding: 7px 12px;
              border: 1px solid var(--line);
              border-radius: 999px;
              background: linear-gradient(135deg, #fff, #f7f6f3);
              color: var(--sub);
              font-size: 13px;
            }
            .grid { display: grid; grid-template-columns: 1.4fr 1fr; gap: 14px; }
            .grid > * { min-width: 0; }
            .stack { display: grid; gap: 14px; }
            .stack > * { min-width: 0; }
            .card {
              background: var(--panel);
              border: 1px solid var(--line);
              border-radius: 12px;
              padding: 14px;
              box-shadow: 0 8px 24px rgba(42, 58, 52, 0.06);
            }
            h2 { margin: 0 0 10px; font-size: 17px; }
            .muted { color: var(--sub); font-size: 12px; }
            .row { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 9px; }
            .single { grid-template-columns: 1fr; }
            label { display: block; font-size: 12px; margin-bottom: 4px; color: var(--sub); }
            input {
              width: 100%;
              border: 1px solid #cdbda8;
              border-radius: 8px;
              padding: 8px 10px;
              background: #fff;
              color: var(--ink);
            }
            input:focus { outline: none; border-color: var(--accent); box-shadow: 0 0 0 3px rgba(13,122,107,0.15); }
            .btns { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 4px; }
            button {
              border: 0;
              border-radius: 8px;
              padding: 8px 12px;
              font-weight: 700;
              cursor: pointer;
              color: #fff;
              background: var(--accent-2);
            }
            button.alt { background: var(--accent); }
            button.dark { background: #314644; }
            button.warn { background: var(--danger); }
            pre {
              margin: 0;
              border-radius: 10px;
              background: #162523;
              color: #d7ebdf;
              border: 1px solid #2b413e;
              padding: 11px;
              min-height: 140px;
              max-height: 360px;
              overflow: auto;
              font-size: 12px;
              line-height: 1.45;
            }
            .cb-head {
              display: flex;
              flex-wrap: wrap;
              align-items: center;
              justify-content: space-between;
              gap: 8px;
              margin-bottom: 10px;
              flex: 0 0 auto;
            }
            .cb-stats { font-size: 12px; color: var(--sub); display: grid; gap: 3px; }
            .cb-card {
              display: flex;
              flex-direction: column;
              min-height: 520px;
            }
            .cb-list {
              display: grid;
              gap: 8px;
              align-content: start;
              height: 390px;
              min-height: 390px;
              overflow-y: scroll;
              overflow-x: hidden;
              scrollbar-gutter: stable;
              padding-right: 2px;
              flex: 1 1 auto;
            }
            .cb-item {
              border: 1px solid #d7cab8;
              border-radius: 10px;
              background: #fff;
              padding: 10px;
              width: 100%;
            }
            .cb-title { display: flex; align-items: center; justify-content: space-between; gap: 8px; margin-bottom: 6px; }
            .tag {
              display: inline-flex;
              align-items: center;
              border-radius: 999px;
              padding: 2px 8px;
              font-size: 12px;
              font-weight: 700;
              color: #fff;
            }
            .tag.mid { background: #5e7b90; }
            .tag.ok { background: var(--ok); }
            .tag.bad { background: var(--danger); }
            .kv { display: grid; grid-template-columns: repeat(2, minmax(0,1fr)); gap: 6px; font-size: 12px; color: #334443; }
            .kv code { font-family: Consolas, "Courier New", monospace; color: #0f3a35; word-break: break-all; }
            details { margin-top: 7px; }
            details summary { cursor: pointer; font-size: 12px; color: #35514d; }
            details pre { margin-top: 6px; min-height: 70px; max-height: 220px; }
            .empty {
              border: 1px dashed #ccbca8;
              border-radius: 10px;
              padding: 20px 12px;
              color: #7a8a84;
              text-align: center;
              font-size: 13px;
              background: #fffdf9;
              min-height: 100%;
              display: flex;
              align-items: center;
              justify-content: center;
            }
            .tips { white-space: pre-line; line-height: 1.5; }
            @media (max-width: 1024px) {
              .grid { grid-template-columns: 1fr; }
            }
            @media (max-width: 740px) {
              .row { grid-template-columns: 1fr; }
            }
          </style>
        </head>
        <body>
          <main class="page">
            <div class="top">
              <h1>RCS / WCS 联调控制台</h1>
              <div class="mode">当前模式：__MODE_LABEL__</div>
            </div>
            <div class="grid">
              <section class="card">
                <h2>任务下发</h2>
                <div class="muted">支持 Mission 与 WMS 任务接口，参数保持兼容。</div>
                <div class="row">
                  <div><label>mission_no</label><input id="missionNo" value="M202604210001"/></div>
                  <div><label>task_no</label><input id="taskNo" value="T202604210001"/></div>
                </div>
                <div class="row single">
                  <div><label>biz_task_no（WMS 任务用）</label><input id="bizTaskNo" value="BIZ-202604210001"/></div>
                </div>
                <div class="row">
                  <div><label>from_point</label><input id="fromPoint" list="pointOptions" value="Point-0020"/></div>
                  <div><label>to_point</label><input id="toPoint" list="pointOptions" value="Point-0026"/></div>
                </div>
                <datalist id="pointOptions"></datalist>
                <div class="row">
                  <div><label>pallet_no</label><input id="palletNo" value="PLT000000123"/></div>
                  <div><label>priority（1-100）</label><input id="priority" value="80"/></div>
                </div>
                <div class="row single">
                  <div><label>callback_url</label><input id="callbackUrl"/></div>
                </div>
                <div class="row">
                  <div><label>X-Trace-Id</label><input id="traceId" placeholder="为空则由调用方自动生成"/></div>
                  <div><label>X-Request-Id</label><input id="requestId" placeholder="为空则由调用方自动生成"/></div>
                </div>
                <div class="btns">
                  <button onclick="createMission()">创建 Mission</button>
                  <button class="alt" onclick="queryMission()">查询 Mission</button>
                  <button class="warn" onclick="cancelMission()">取消 Mission</button>
                  <button class="dark" onclick="injectFinishedEvent()">注入 FINISHED 事件</button>
                  <button onclick="createInboundTask()">创建入库任务</button>
                  <button class="alt" onclick="createOutboundTask()">创建出库任务</button>
                  <button class="alt" onclick="queryWmsTask()">查询 WMS 任务</button>
                  <button class="warn" onclick="cancelWmsTask()">取消 WMS 任务</button>
                  <button class="dark" onclick="genIds()">生成新 ID</button>
                </div>
              </section>

              <div class="stack">
                <section class="card">
                  <h2>接口响应日志</h2>
                  <pre id="respLog"></pre>
                </section>

                <section class="card cb-card">
                  <h2>回调收件箱（WCS Callback Inbox）</h2>
                  <div class="cb-head">
                    <div class="cb-stats">
                      <div id="cbCount">共 0 条</div>
                      <div id="cbState">等待加载</div>
                    </div>
                    <div class="btns" style="margin-top:0;">
                      <button class="alt" onclick="loadCallbacks()">刷新</button>
                      <button class="warn" onclick="clearCallbacks()">清空</button>
                    </div>
                  </div>
                  <div class="cb-list" id="cbList"></div>
                </section>
              </div>

              <section class="card">
                <h2>openTCS Web API 查询</h2>
                <div class="row single">
                  <div><label>openTCS base URL</label><input id="openTcsBaseUrl"/></div>
                </div>
                <div class="row single">
                  <div><label>X-Api-Access-Key（可选）</label><input id="openTcsAccessKey" placeholder="可留空"/></div>
                </div>
                <div class="btns">
                  <button class="alt" onclick="queryKernelVersion()">查询内核版本</button>
                  <button class="alt" onclick="queryVehicles()">查询车辆</button>
                  <button class="alt" onclick="queryTransportOrders()">查询订单</button>
                  <button class="alt" onclick="queryPlantModel()">查询地图模型</button>
                  <button class="dark" onclick="loadPointSuggestions()">加载点位建议</button>
                  <button class="dark" onclick="validateMissionPoints()">校验点位参数</button>
                </div>
                <pre id="openTcsLog"></pre>
              </section>

              <section class="card">
                <h2>联调检查清单</h2>
                <div class="tips">1. 创建任务，确认接口返回成功
        2. 在 openTCS 查询到对应 transport order
        3. 订单推进到 FINISHED（或注入 FINISHED 事件）
        4. 查询任务状态，确认 DONE / FAILED
        5. 回调收件箱可见终态事件（例如 DROPPED）</div>
              </section>
            </div>
          </main>
          <script>
            const $ = (id) => document.getElementById(id);
            const missionNo = () => $('missionNo').value.trim();
            const taskNo = () => $('taskNo').value.trim();
            const bizTaskNo = () => $('bizTaskNo').value.trim();
            const nowText = () => new Date().toLocaleString('zh-CN', { hour12: false });
            const esc = (value) => String(value ?? '')
                .replaceAll('&', '&amp;')
                .replaceAll('<', '&lt;')
                .replaceAll('>', '&gt;')
                .replaceAll('"', '&quot;')
                .replaceAll("'", '&#39;');
            const safeJsonParse = (text) => {
              if (typeof text !== 'string') return null;
              try { return JSON.parse(text); } catch { return null; }
            };
            const eventTagClass = (eventType) => {
              const t = String(eventType || '').toUpperCase();
              if (t.includes('FAIL') || t.includes('CANCEL') || t.includes('ERROR')) return 'bad';
              if (t.includes('DONE') || t.includes('DROP') || t.includes('FINISH')) return 'ok';
              return 'mid';
            };
            const commonHeaders = () => {
              const h = {'Content-Type':'application/json'};
              const t = $('traceId').value.trim();
              const r = $('requestId').value.trim();
              if (t) h['X-Trace-Id'] = t;
              if (r) h['X-Request-Id'] = r;
              return h;
            };
            const logResp = (title, payload, headers) => {
              const lines = [
                '[' + new Date().toISOString() + '] ' + title,
                'trace=' + (headers?.get('X-Trace-Id') || '<none>'),
                'request=' + (headers?.get('X-Request-Id') || '<none>'),
                JSON.stringify(payload, null, 2),
                ''
              ];
              $('respLog').textContent = lines.join('\\n') + $('respLog').textContent;
            };
            const reqJson = async (url, init, title) => {
              const resp = await fetch(url, init);
              let data = null;
              try {
                data = await resp.json();
              }
              catch {
                data = { raw: await resp.text() };
              }
              logResp(title + ' [HTTP ' + resp.status + ']', data, resp.headers);
              return data;
            };
            const openTcsBaseUrl = () => $('openTcsBaseUrl').value.trim().replace(/\\/$/, '');
            const openTcsHeaders = () => {
              const h = {'Accept':'application/json'};
              const key = $('openTcsAccessKey').value.trim();
              if (key) h['X-Api-Access-Key'] = key;
              return h;
            };
            const logOpenTcs = (title, payload, status) => {
              const lines = [
                '[' + new Date().toISOString() + '] ' + title + ' [HTTP ' + status + ']',
                JSON.stringify(payload, null, 2),
                ''
              ];
              $('openTcsLog').textContent = lines.join('\\n') + $('openTcsLog').textContent;
            };
            const renderCallbacks = (data, initial = false) => {
              const items = Array.isArray(data?.data) ? data.data : [];
              $('cbCount').textContent = '共 ' + items.length + ' 条';
              if (items.length === 0) {
                $('cbState').textContent = initial ? '等待回调事件' : '已清空，等待新回调';
                $('cbList').innerHTML = '<div class="empty">暂无回调记录。可先创建任务，再点击“刷新”。</div>';
                return;
              }
              const latest = items[items.length - 1];
              $('cbState').textContent = '页面更新时间：' + nowText() + '，最新回调：' + (latest.receivedAt || '-');
              const html = items.slice().reverse().map((item, index) => {
                const payloadObj = safeJsonParse(item.payload) || {};
                const eventType = payloadObj.event_type || payloadObj.result_type || 'UNKNOWN';
                const mission = payloadObj.mission_no || '-';
                const task = payloadObj.task_no || '-';
                const agv = payloadObj.agv_id || '-';
                const point = payloadObj.point_id || '-';
                return `
                  <article class="cb-item">
                    <div class="cb-title">
                      <strong>#${items.length - index}</strong>
                      <span class="tag ${eventTagClass(eventType)}">${esc(eventType)}</span>
                    </div>
                    <div class="kv">
                      <div>mission_no: <code>${esc(mission)}</code></div>
                      <div>task_no: <code>${esc(task)}</code></div>
                      <div>agv_id: <code>${esc(agv)}</code></div>
                      <div>point_id: <code>${esc(point)}</code></div>
                      <div>receivedAt: <code>${esc(item.receivedAt || '-')}</code></div>
                    </div>
                    <details>
                      <summary>查看原始回调载荷</summary>
                      <pre>${esc(item.payload || '')}</pre>
                    </details>
                  </article>
                `;
              }).join('');
              $('cbList').innerHTML = html;
            };
            async function queryOpenTcs(path, title){
              const url = openTcsBaseUrl() + path;
              try {
                const resp = await fetch(url, { method:'GET', headers: openTcsHeaders() });
                let data = null;
                try { data = await resp.json(); }
                catch { data = { raw: await resp.text() }; }
                logOpenTcs(title, data, resp.status);
                return data;
              }
              catch(err) {
                const data = { error: String(err), url };
                logOpenTcs(title, data, 0);
                return null;
              }
            }
            async function queryKernelVersion(){ await queryOpenTcs('/v1/kernel/version', 'openTCS 内核版本'); }
            async function queryVehicles(){ await queryOpenTcs('/v1/vehicles', 'openTCS 车辆列表'); }
            async function queryTransportOrders(){ await queryOpenTcs('/v1/transportOrders', 'openTCS 订单列表'); }
            async function queryPlantModel(){ await queryOpenTcs('/v1/plantModel', 'openTCS 地图模型'); }
            function setPointOptions(pointNames){
              const list = $('pointOptions');
              list.innerHTML = '';
              pointNames.forEach((name) => {
                const opt = document.createElement('option');
                opt.value = name;
                list.appendChild(opt);
              });
            }
            async function loadPointSuggestions(){
              const data = await queryOpenTcs('/v1/plantModel', 'openTCS 地图模型（加载点位建议）');
              const points = Array.isArray(data?.points) ? data.points : [];
              const names = points.map((p) => p?.name).filter(Boolean);
              setPointOptions(names);
              if (names.length > 0) {
                if (!$('fromPoint').value.trim()) $('fromPoint').value = names[0];
                if (names.length > 1 && !$('toPoint').value.trim()) $('toPoint').value = names[1];
              }
              logOpenTcs('点位建议已加载', { count: names.length, sample: names.slice(0, 20) }, 200);
            }
            async function validateMissionPoints(){
              const from = $('fromPoint').value.trim();
              const to = $('toPoint').value.trim();
              const data = await queryOpenTcs('/v1/plantModel', '校验任务点位');
              const pointNames = new Set((Array.isArray(data?.points) ? data.points : []).map((p) => p?.name).filter(Boolean));
              const result = {
                from_point: from,
                to_point: to,
                from_exists: pointNames.has(from),
                to_exists: pointNames.has(to),
                total_points: pointNames.size
              };
              logOpenTcs('任务点位校验结果', result, 200);
            }
            async function createMission(){
              const body = {
                mission_no: missionNo(),
                task_no: taskNo(),
                from_point: $('fromPoint').value.trim(),
                to_point: $('toPoint').value.trim(),
                pallet_no: $('palletNo').value.trim(),
                priority: Number($('priority').value || '50'),
                callback_url: $('callbackUrl').value.trim()
              };
              await reqJson('/api/v1/wcs/agv/missions', { method:'POST', headers: commonHeaders(), body: JSON.stringify(body) }, '创建 Mission');
            }
            async function queryMission(){
              await reqJson('/api/v1/wcs/agv/missions/' + encodeURIComponent(missionNo()), { method:'GET', headers: commonHeaders() }, '查询 Mission');
            }
            async function cancelMission(){
              await reqJson('/api/v1/wcs/agv/missions/' + encodeURIComponent(missionNo()) + '/cancel', { method:'POST', headers: commonHeaders(), body: '' }, '取消 Mission');
            }
            async function injectFinishedEvent(){
              const body = {
                eventTime: new Date().toISOString().replace('.000Z','Z'),
                currentObjectState: {
                  name: missionNo(),
                  state: 'FINISHED',
                  currentDriveOrderIndex: 1,
                  processingVehicle: 'AGV_01',
                  properties: { task_no: taskNo() }
                }
              };
              await reqJson('/api/v1/opentcs/events/transport-orders', { method:'POST', headers: commonHeaders(), body: JSON.stringify(body) }, '注入 FINISHED 事件');
            }
            async function createInboundTask(){
              const body = {
                biz_task_no: bizTaskNo(),
                mission_no: missionNo(),
                task_no: taskNo(),
                from_point: $('fromPoint').value.trim(),
                to_point: $('toPoint').value.trim(),
                pallet_no: $('palletNo').value.trim(),
                priority: Number($('priority').value || '50'),
                callback_url: $('callbackUrl').value.trim()
              };
              await reqJson('/api/v1/wcs/inbound/tasks', { method:'POST', headers: commonHeaders(), body: JSON.stringify(body) }, '创建入库任务');
            }
            async function createOutboundTask(){
              const body = {
                biz_task_no: bizTaskNo(),
                mission_no: missionNo(),
                task_no: taskNo(),
                from_point: $('fromPoint').value.trim(),
                to_point: $('toPoint').value.trim(),
                pallet_no: $('palletNo').value.trim(),
                priority: Number($('priority').value || '50'),
                callback_url: $('callbackUrl').value.trim()
              };
              await reqJson('/api/v1/wcs/outbound/tasks', { method:'POST', headers: commonHeaders(), body: JSON.stringify(body) }, '创建出库任务');
            }
            async function queryWmsTask(){
              await reqJson('/api/v1/wcs/tasks/' + encodeURIComponent(bizTaskNo()), { method:'GET', headers: commonHeaders() }, '查询 WMS 任务');
            }
            async function cancelWmsTask(){
              await reqJson('/api/v1/wcs/tasks/' + encodeURIComponent(bizTaskNo()) + '/cancel', { method:'POST', headers: commonHeaders(), body: '' }, '取消 WMS 任务');
            }
            async function loadCallbacks(){
              const data = await reqJson('/demo/wcs/callbacks', { method:'GET' }, '查询回调收件箱');
              renderCallbacks(data);
            }
            async function clearCallbacks(){
              await reqJson('/demo/wcs/callbacks/clear', { method:'POST' }, '清空回调收件箱');
              await loadCallbacks();
            }
            function genIds(){
              const s = Date.now().toString();
              $('missionNo').value = 'M' + s;
              $('taskNo').value = 'T' + s;
              $('bizTaskNo').value = 'BIZ-' + s;
              $('traceId').value = 'trace-' + s;
              $('requestId').value = 'req-' + s;
            }
            window.addEventListener('DOMContentLoaded', () => {
              $('callbackUrl').value = window.location.origin + '/demo/wcs/callback';
              $('openTcsBaseUrl').value = '__DEFAULT_OPENTCS_BASE_URL__';
              renderCallbacks({ data: [] }, true);
              $('openTcsLog').textContent = JSON.stringify(
                { hint: '先点“加载点位建议”，再创建任务；回调区点击“刷新”可查看最新状态。' },
                null,
                2
              );
            });
          </script>
        </body>
        </html>
        """
        .replace("__MODE_LABEL__", modeLabel)
        .replace("__DEFAULT_OPENTCS_BASE_URL__", openTcsBaseUrlJs);
  }

  private static String escapeHtml(String input) {
    if (input == null || input.isEmpty()) {
      return "";
    }
    return input
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace("\"", "&quot;")
        .replace("'", "&#39;");
  }

  private static String escapeJsSingleQuoted(String input) {
    if (input == null || input.isEmpty()) {
      return "";
    }
    return input
        .replace("\\", "\\\\")
        .replace("'", "\\'")
        .replace("\r", "\\r")
        .replace("\n", "\\n");
  }

  public record DemoCallbackRecord(
      String receivedAt,
      String payload,
      String traceId,
      String requestId
  ) {
  }
}
