import { createServer as createHttpServer } from "node:http";
import { request as httpRequest } from "node:http";
import { request as httpsRequest } from "node:https";
import { execFile as nodeExecFile } from "node:child_process";
import { readFile } from "node:fs/promises";
import { extname, join, normalize, resolve } from "node:path";
import { promisify } from "node:util";

import { defaultSeed } from "./default-seed.mjs";
import { createLarkBaseStore } from "./lark-base-store.mjs";
import { normalizeState, setCompletion, upsertDevice, checkAndNotifyOverdueTasks } from "./state-utils.mjs";

const defaultHomeId = process.env.LIFETODO_HOME_ID || "demo-home";
const defaultAlertChatId = "oc_e0fade30cf1d453b162f7a8748d3bab9";
const staticRoot = resolve("apps/pwa-prototype");
const execFile = promisify(nodeExecFile);

export function createApiHandler({ store, seed = defaultSeed, now = () => new Date(), notifySyncFailure = sendLarkAlert, notifyAlert = sendLarkAlert } = {}) {
  const alertNotifier = notifyAlert || notifySyncFailure;
  return async function handle(request) {
    const url = new URL(request.url);
    const homeId = url.searchParams.get("home") || defaultHomeId;

    if (request.method === "OPTIONS") {
      return jsonResponse(null, 204);
    }

    try {
      if (url.pathname === "/api/state" && request.method === "GET") {
        const state = await readOrSeed(store, homeId, seed);
        await checkAndNotifyOverdueTasks(state, homeId, alertNotifier, now);
        return jsonResponse({ homeId, state });
      }

      if (url.pathname === "/api/state" && request.method === "PUT") {
        const body = await request.json();
        const state = normalizeState(body.state, now);
        await store.writeState(homeId, state, body.source || "app-h5");
        return jsonResponse({ homeId, state });
      }

      if (url.pathname === "/api/completions" && request.method === "POST") {
        const body = await request.json();
        if (!body.taskId || !body.date) {
          return jsonResponse({ error: "taskId and date are required" }, 400);
        }
        const state = await readOrSeed(store, homeId, seed);
        setCompletion(state, body.taskId, body.date, body.completed !== false, body.source || "api", now);
        await checkAndNotifyOverdueTasks(state, homeId, alertNotifier, now);
        await store.writeState(homeId, state, body.source || "api");
        return jsonResponse({ homeId, state });
      }

      if (url.pathname === "/api/tasks/check-overdue" && (request.method === "POST" || request.method === "GET")) {
        const state = await readOrSeed(store, homeId, seed);
        const notifiedCount = await checkAndNotifyOverdueTasks(state, homeId, alertNotifier, now);
        return jsonResponse({ homeId, notifiedCount, ok: true });
      }

      if (url.pathname === "/api/devices/heartbeat" && request.method === "POST") {
        const body = await request.json();
        if (!body.deviceId) {
          return jsonResponse({ error: "deviceId is required" }, 400);
        }
        const state = await readOrSeed(store, homeId, seed);
        state.devices = upsertDevice(state.devices, {
          id: body.deviceId,
          name: body.name || body.deviceId,
          location: body.location || "",
          model: body.model || "ESP32-S3-Touch-LCD-4.3",
          status: "online",
          lastSeenAt: now().toISOString()
        });
        await store.writeState(homeId, state, body.source || "device-heartbeat");
        return jsonResponse({ homeId, state });
      }

      if (url.pathname === "/api/devices/sync-failure" && request.method === "POST") {
        const body = await request.json();
        if (!body.deviceId) {
          return jsonResponse({ error: "deviceId is required" }, 400);
        }
        const text = [
          "LifeTodo 设备同步失败",
          `设备：${body.deviceId}`,
          `家庭：${homeId}`,
          `连续失败：${body.failures || 1} 次`,
          `错误：${body.error || "unknown"}`,
          `时间：${now().toISOString()}`
        ].join("\n");
        const notified = await notifySyncFailure(text, body);
        return jsonResponse({ ok: true, notified });
      }

      return jsonResponse({ error: "Not found" }, 404);
    } catch (error) {
      return jsonResponse({ error: error.message || "Internal server error" }, 500);
    }
  };
}

async function sendLarkAlert(text) {
  const chatId = process.env.LIFETODO_LARK_ALERT_CHAT_ID || defaultAlertChatId;
  if (chatId) {
    await execFile("lark-cli", [
      "im",
      "+messages-send",
      "--chat-id",
      chatId,
      "--text",
      text,
      "--as",
      process.env.LIFETODO_LARK_ALERT_AS || "user",
      "--format",
      "json"
    ]);
    return true;
  }

  const webhook = process.env.LIFETODO_LARK_ALERT_WEBHOOK;
  if (!webhook) return false;
  await postJson(webhook, {
    msg_type: "text",
    content: { text }
  });
  return true;
}

async function postJson(url, body) {
  const endpoint = new URL(url);
  const payload = Buffer.from(JSON.stringify(body));
  const requestImpl = endpoint.protocol === "https:" ? httpsRequest : httpRequest;
  await new Promise((resolvePromise, reject) => {
    const request = requestImpl(
      {
        method: "POST",
        hostname: endpoint.hostname,
        port: endpoint.port || (endpoint.protocol === "https:" ? 443 : 80),
        path: `${endpoint.pathname}${endpoint.search}`,
        headers: {
          "Content-Type": "application/json",
          "Content-Length": payload.length
        },
        timeout: 8000
      },
      (response) => {
        response.resume();
        response.on("end", () => {
          if (response.statusCode >= 200 && response.statusCode < 300) {
            resolvePromise();
          } else {
            reject(new Error(`Lark alert webhook returned ${response.statusCode}`));
          }
        });
      }
    );
    request.on("timeout", () => request.destroy(new Error("Lark alert webhook timed out")));
    request.on("error", reject);
    request.end(payload);
  });
}

export function createLifeTodoServer({ store = createLarkBaseStore(), seed = defaultSeed, now } = {}) {
  const apiHandler = createApiHandler({ store, seed, now });
  return createHttpServer(async (request, response) => {
    try {
      const url = new URL(request.url, "http://localhost");
      const apiResponse = url.pathname.startsWith("/api/") ? await apiHandler(toApiRequest(request)) : await staticResponse(url);
      response.writeHead(apiResponse.status, Object.fromEntries(apiResponse.headers));
      response.end(Buffer.from(await apiResponse.arrayBuffer()));
    } catch (error) {
      const apiResponse = jsonResponse({ error: error.message || "Internal server error" }, 500);
      response.writeHead(apiResponse.status, Object.fromEntries(apiResponse.headers));
      response.end(Buffer.from(await apiResponse.arrayBuffer()));
    }
  });
}

async function readOrSeed(store, homeId, seed) {
  const state = await store.readState(homeId, seed);
  if (state) return normalizeState(state);
  return normalizeState(seed);
}

function jsonResponse(body, status = 200) {
  return new SimpleResponse(body === null ? "" : JSON.stringify(body), {
    status,
    headers: {
      "Access-Control-Allow-Origin": "*",
      "Access-Control-Allow-Methods": "GET,PUT,POST,OPTIONS",
      "Access-Control-Allow-Headers": "Content-Type",
      "Content-Type": "application/json; charset=utf-8"
    }
  });
}

function toApiRequest(request) {
  return {
    url: `http://${request.headers.host || "localhost"}${request.url}`,
    method: request.method,
    headers: request.headers,
    async json() {
      return JSON.parse(await readRequestBody(request));
    }
  };
}

async function readRequestBody(request) {
  const chunks = [];
  for await (const chunk of request) {
    chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk));
  }
  return Buffer.concat(chunks).toString("utf8");
}

class SimpleResponse {
  constructor(body = "", { status = 200, headers = {} } = {}) {
    this.status = status;
    this.headers = new Map(Object.entries(headers));
    this.body = Buffer.isBuffer(body) ? body : Buffer.from(String(body));
  }

  async arrayBuffer() {
    return this.body.buffer.slice(this.body.byteOffset, this.body.byteOffset + this.body.byteLength);
  }

  async json() {
    return JSON.parse(this.body.toString("utf8"));
  }
}

const defaultStaticHeaders = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET,PUT,POST,OPTIONS",
  "Access-Control-Allow-Headers": "Content-Type"
};

async function staticResponse(url) {
  const pathname = url.pathname === "/" ? "/index.html" : url.pathname;
  const filePath = normalize(join(staticRoot, pathname));
  if (!filePath.startsWith(staticRoot)) {
    return new SimpleResponse("Not found", { status: 404 });
  }
  try {
    const body = await readFile(filePath);
    return new SimpleResponse(body, { headers: { ...defaultStaticHeaders, "Content-Type": contentType(filePath) } });
  } catch {
    return new SimpleResponse("Not found", { status: 404 });
  }
}

function contentType(filePath) {
  return {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".webmanifest": "application/manifest+json; charset=utf-8"
  }[extname(filePath)] || "application/octet-stream";
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const port = Number(process.env.PORT || 8787);
  createLifeTodoServer().listen(port, () => {
    console.log(`LifeTodo API listening on http://localhost:${port}`);
  });
}
