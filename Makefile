CXX = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icpp

.PHONY: all test test-wasm local-harness qollab-bundle wasm serve clean
all: local-harness
build:
	mkdir -p build

build/test_core: cpp/core.hpp cpp/reference16.hpp cpp/test_core.cpp | build
	$(CXX) $(CXXFLAGS) cpp/test_core.cpp -o $@

build/test_core64: cpp/core.hpp cpp/reference64.hpp cpp/test_core.cpp | build
	$(CXX) $(CXXFLAGS) -DUSE_GALAXY64 cpp/test_core.cpp -o $@

build/test_session: cpp/core.hpp cpp/reference64.hpp cpp/listening.hpp cpp/session.hpp cpp/test_session.cpp | build
	$(CXX) $(CXXFLAGS) cpp/test_session.cpp -o $@

build/test_balance: cpp/core.hpp cpp/reference64.hpp cpp/listening.hpp cpp/test_balance.cpp | build
	$(CXX) $(CXXFLAGS) cpp/test_balance.cpp -o $@

build/test_tone: cpp/core.hpp cpp/reference64.hpp cpp/test_tone.cpp | build
	$(CXX) $(CXXFLAGS) cpp/test_tone.cpp -o $@

build/test_character: cpp/core.hpp cpp/reference64.hpp cpp/test_character.cpp | build
	$(CXX) $(CXXFLAGS) cpp/test_character.cpp -o $@

build/test_stereo: cpp/core.hpp cpp/reference64.hpp cpp/test_stereo.cpp | build
	$(CXX) $(CXXFLAGS) cpp/test_stereo.cpp -o $@

build/test_wasm_api: cpp/core.hpp cpp/reference64.hpp cpp/listening.hpp cpp/session.hpp cpp/wasm_api.cpp cpp/test_wasm_api.cpp | build
	$(CXX) $(CXXFLAGS) cpp/test_wasm_api.cpp -o $@

EMXX ?= em++
WASM_EXPORTS = _ms_events,_ms_event_count,_ms_dropped_events,_ms_create,_ms_source_ptr,_ms_source_max_frames,_ms_rooms,_ms_probabilities,_ms_left,_ms_right,_ms_telemetry,_ms_error,_ms_max_frames,_ms_telemetry_floats,_ms_room_floats,_ms_max_rooms,_ms_prepare,_ms_command,_ms_process,_ms_destroy
# Local preview works from the supplied, source-checked single-file WASM.
build/wasm/manyspace-core.js: prebuilt/manyspace-core.js prebuilt/manifest.json cpp/core.hpp cpp/listening.hpp cpp/session.hpp cpp/wasm_api.cpp | build
	python3 tools/use_prebuilt.py

# Explicit recompilation requires em++ (Emscripten 6.0.12 was used).
wasm: | build
	mkdir -p build/wasm
	$(EMXX) -std=c++17 -O2 -Icpp cpp/wasm_api.cpp -o build/wasm/manyspace-core.js \
	  -sMODULARIZE=1 -sEXPORT_NAME=createManyspace -sSINGLE_FILE=1 -sENVIRONMENT=web,node \
	  -sEXPORTED_FUNCTIONS=$(WASM_EXPORTS) -sEXPORTED_RUNTIME_METHODS=HEAPF32,UTF8ToString \
	  -sALLOW_MEMORY_GROWTH=0 -sINITIAL_MEMORY=67108864 -sSTACK_SIZE=1048576 -sDISABLE_EXCEPTION_CATCHING=0 -sFILESYSTEM=0
	@ls -la build/wasm/manyspace-core.js

qollab-bundle: build/wasm/manyspace-core.js | build
	mkdir -p build/qollab/manyspace build/qollab/presets build/qollab/assets
	cp web/notebook.html build/qollab/index.html
	cat web/notebook-fonts.css web/notebook.css web/notebook-art.css > build/qollab/main.css
	cat web/model.js web/wasm-host.js web/notebook-templates.js web/notebook-parameters.js web/notebook-art.js web/notebook.js web/notebook-entry.js > build/qollab/main.js
	cp manyspace/*.py build/qollab/manyspace/
	cp presets/reference64.json build/qollab/presets/
	cp build/wasm/manyspace-core.js build/qollab/assets/
	cp web/fonts/*-LICENSE.txt web/licenses/*-LICENSE.txt build/qollab/assets/
	cp web/qollab-requirements.txt build/qollab/requirements.txt
	@find build/qollab -type f -size +2M -print -exec false {} + || (echo "A bundled file exceeds 2 MB" && exit 1)
	@echo "bundle ok: $$(find build/qollab -type f | wc -l | tr -d ' ') files"

# Local browser check of the hosted UI without Qollab: serve build/local over HTTP.
local-harness: build/wasm/manyspace-core.js | build
	mkdir -p build/local
	cat web/notebook-fonts.css web/notebook.css web/notebook-art.css > build/local/main.css
	cat web/model.js web/wasm-host.js web/notebook-templates.js web/notebook-parameters.js web/notebook-art.js web/notebook.js web/notebook-local.js > build/local/app.js
	cp build/wasm/manyspace-core.js presets/reference64.json presets/reference16.json build/local/
	cp tests/notebook-viewport.html build/local/viewport.html
	cp tests/notebook-fixture.html build/local/fixture.html
	printf '%s\n' '<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Manyfold · local notebook</title><link rel="stylesheet" href="main.css"></head><body>' \
	  '<div id="app"></div><script>fetch("reference64.json").then(r=>r.json()).then(r=>{window.ManyspaceReference=r;const s=document.createElement("script");s.src="app.js";document.body.append(s);});</script></body></html>' > build/local/index.html
	@echo "open http://127.0.0.1:8765 after: python3 tools/notebook_server.py"

test: build/test_core build/test_core64 build/test_session build/test_balance build/test_tone build/test_stereo build/test_character build/test_wasm_api build/wasm/manyspace-core.js
	python3 -m unittest discover -s tests -p 'test_*.py' -v
	node tests/test_workflow.js
	node tools/test_fixtures.js
	node tests/test_notebook.js
	node tests/test_notebook_local.js
	node tests/test_notebook_art.js
	./build/test_core
	./build/test_core64
	./build/test_session
	./build/test_balance
	./build/test_tone
	./build/test_stereo
	./build/test_character
	./build/test_wasm_api
	node tests/test_wasm_core.js
	node tests/test_notebook_audio.js

test-wasm: build/wasm/manyspace-core.js build/test_wasm_api
	node tests/test_workflow.js
	node tools/test_fixtures.js
	./build/test_wasm_api
	node tests/test_wasm_core.js
	node tests/test_notebook_audio.js

serve: local-harness
	python3 tools/notebook_server.py

clean:
	rm -rf build
