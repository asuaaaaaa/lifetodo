export function createClientId(prefix = "id", cryptoLike = globalThis.crypto, now = () => Date.now()) {
  if (cryptoLike && typeof cryptoLike.randomUUID === "function") {
    return cryptoLike.randomUUID();
  }

  const timePart = String(now());
  let randomPart = "";
  if (cryptoLike && typeof cryptoLike.getRandomValues === "function") {
    const values = new Uint32Array(2);
    cryptoLike.getRandomValues(values);
    randomPart = Array.from(values, (value) => value.toString(36)).join("");
  } else {
    randomPart = Math.random().toString(36).slice(2, 12);
  }

  return `${prefix}_${timePart}_${randomPart || "0"}`;
}
