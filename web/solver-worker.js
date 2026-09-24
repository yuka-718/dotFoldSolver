let modulePromise;

async function getModule() {
  if (!modulePromise) {
    importScripts("./solver.js");
    modulePromise = createDotFoldSolver({
      locateFile: (path) => new URL(path, self.location.href).href,
    });
  }
  return modulePromise;
}

getModule()
  .then(() => self.postMessage({ type: "ready" }))
  .catch((error) => self.postMessage({ type: "error", error: String(error) }));

self.onmessage = async ({ data }) => {
  const { id, type, dots, skip = 1 } = data;
  try {
    const module = await getModule();
    if (type === "analyze") {
      const value = module.ccall("dotfold_loop_length", "number", ["string"], [dots]);
      self.postMessage({ id, type: "analysis", value });
      return;
    }
    if (type === "solve") {
      const output = module.ccall(
        "dotfold_solve",
        "string",
        ["string", "number"],
        [dots, skip],
      );
      self.postMessage({ id, type: "solution", output });
    }
  } catch (error) {
    self.postMessage({ id, type: "error", error: String(error) });
  }
};
