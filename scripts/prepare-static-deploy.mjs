import { cp, mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const source = resolve(root, "apps/pwa-prototype");
const output = resolve(root, "dist/site");

await rm(output, { force: true, recursive: true });
await mkdir(resolve(output, "app"), { recursive: true });
await mkdir(resolve(output, "device"), { recursive: true });
await mkdir(resolve(output, "design-system"), { recursive: true });

await writeDeployHtml(resolve(source, "index.html"), resolve(output, "app/index.html"));
await writeDeployHtml(resolve(source, "device.html"), resolve(output, "device/index.html"));
await cp(resolve(source, "styles.css"), resolve(output, "app/styles.css"));
await cp(resolve(source, "styles.css"), resolve(output, "device/styles.css"));
await cp(resolve(source, "app.js"), resolve(output, "app/app.js"));
await cp(resolve(source, "app.js"), resolve(output, "device/app.js"));
await cp(resolve(source, "firebase-store.js"), resolve(output, "app/firebase-store.js"));
await cp(resolve(source, "firebase-store.js"), resolve(output, "device/firebase-store.js"));
await cp(resolve(source, "manifest.webmanifest"), resolve(output, "app/manifest.webmanifest"));
await cp(resolve(root, "design-system/tokens.css"), resolve(output, "design-system/tokens.css"));
await cp(resolve(root, "design-system/tokens.json"), resolve(output, "design-system/tokens.json"));

console.log(`Prepared static deploy at ${output}`);

async function writeDeployHtml(input, target) {
  const html = await readFile(input, "utf8");
  await writeFile(target, html.replace("../../design-system/tokens.css", "../design-system/tokens.css"));
}
