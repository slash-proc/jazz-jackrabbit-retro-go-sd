import createJJModule from "./jj_wasm.mjs";

const wasmUrl = new URL("./jj_wasm.wasm", import.meta.url).href;

self.onmessage = async event => {
  let module = null;
  try {
    module = await createJJModule({
      noInitialRun: true,
      locateFile: () => wasmUrl,
      print: line => postMessage({ type: "log", message: line }),
      printErr: line => postMessage({ type: "log", message: line }),
    });
    module.FS.mkdir("/work");
    module.FS.mkdir("/work/music");
    module.FS.writeFile("/work/song.jj", new Uint8Array(event.data.pack));
    try {
      module.callMain(["/work/song.jj", "-R", "/work/music", "-M", "11025", "-A"]);
    } catch (error) {
      // Emscripten raises ExitStatus when callMain returns from main().
      if (error?.name !== "ExitStatus" || error.status !== 0) throw error;
    }
    const name = module.FS.readdir("/work/music").find(item => item.endsWith(".MUS"));
    if (!name) throw new Error("The renderer did not produce a music file.");
    const music = module.FS.readFile(`/work/music/${name}`);
    const transferable = music.buffer.slice(music.byteOffset, music.byteOffset + music.byteLength);
    postMessage({ type: "done", music: transferable }, [transferable]);
  } catch (error) {
    postMessage({ type: "error", message: error?.message || String(error) });
  } finally {
    module?.FS.quit?.();
    module = null;
  }
};
