import { createServer as createHttpServer } from "node:http";
import { readFile } from "node:fs/promises";
import { extname, join, normalize, resolve } from "node:path";

import { defaultSeed } from "./default-seed.mjs";
import { createLarkBaseStore } from "./lark-base-store.mjs";
import { normalizeState, setCompletion, upsertDevice } from "./state-utils.mjs";

const defaultHomeId = process.env.LIFETODO_HOME_ID || "demo-home";
const staticRoot = resolve("apps/pwa-prototype");

export function createApiHandler({ store, seed = defaultSeed, now = () => new Date() }) {
  return async function handle(request) {
    const url = new URL(request.url);
    const homeId = url.searchParams.get("home") || defaultHomeId;

    if (request.method === "OPTIONS") {
      return jsonResponse(null, 204);
    }

    try {
      if (url.pathname === "/api/state" && request.method === "GET") {
        const state = await readOrSeed(store, homeId, seed);
        return jsonResponse({ homeId, state });
      }

      if (url.pathname === "/api/state" && request.method === "PUT") {
        const body = await request.json();
        const state = normalizeState(body.state);
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
        await store.writeState(homeId, state, body.source || "api");
        return jsonResponse({ homeId, state });
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

      return jsonResponse({ error: "Not found" }, 404);
    } catch (error) {
      return jsonResponse({ error: error.message || "Internal server error" }, 500);
    }
  };
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
