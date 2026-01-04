import fs from 'fs-extra';
import path from 'path';
import chalk from 'chalk';
import ora from 'ora';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

interface CreateOptions {
  template: string;
  lang: string;
  ui: boolean;
}

export async function create(name: string, options: CreateOptions) {
  const projectDir = path.resolve(process.cwd(), name);

  if (await fs.pathExists(projectDir)) {
    console.error(chalk.red(`Error: Directory "${name}" already exists`));
    process.exit(1);
  }

  const spinner = ora(`Creating WCLAP project "${name}"...`).start();

  try {
    // Create project directory
    await fs.ensureDir(projectDir);

    // Create manifest
    const manifest = {
      wclap_version: '1.0',
      id: `com.example.${name.toLowerCase().replace(/[^a-z0-9]/g, '-')}`,
      name: name,
      vendor: 'Your Name',
      version: '1.0.0',
      description: `${name} - A WCLAP audio plugin`,
      url: '',
      features: ['audio-effect', 'stereo'],
      ui: options.ui ? {
        entry: 'ui/index.html',
        width: 600,
        height: 400
      } : undefined,
      parameters: [
        {
          id: 0,
          name: 'Gain',
          min: 0,
          max: 1,
          default: 0.5
        }
      ]
    };

    await fs.writeJson(path.join(projectDir, 'manifest.json'), manifest, { spaces: 2 });

    // Create DSP source based on language
    if (options.lang === 'rust') {
      await createRustDsp(projectDir, name);
    } else if (options.lang === 'cpp') {
      await createCppDsp(projectDir, name);
    } else if (options.lang === 'assemblyscript') {
      await createAsDsp(projectDir, name);
    }

    // Create UI if requested
    if (options.ui) {
      await createUi(projectDir, name, options.template);
    }

    // Create README
    await fs.writeFile(path.join(projectDir, 'README.md'), `# ${name}

A WCLAP audio plugin.

## Building

### DSP (${options.lang})
${getBuildInstructions(options.lang)}

### UI
${options.ui ? 'Open ui/index.html or run a dev server.' : 'No UI scaffolded.'}

## Development

Use \`clasp-host\` in your DAW for hot-reload during development.
`);

    spinner.succeed(chalk.green(`Created WCLAP project: ${name}`));

    console.log('\nNext steps:');
    console.log(chalk.cyan(`  cd ${name}`));
    console.log(chalk.cyan(getBuildInstructions(options.lang, true)));

  } catch (error) {
    spinner.fail(chalk.red('Failed to create project'));
    console.error(error);
    process.exit(1);
  }
}

async function createRustDsp(projectDir: string, name: string) {
  const cargoToml = `[package]
name = "${name.toLowerCase().replace(/[^a-z0-9]/g, '_')}"
version = "0.1.0"
edition = "2021"

[lib]
crate-type = ["cdylib"]

[dependencies]
wclap = "0.1"

[profile.release]
lto = true
opt-level = "z"
`;

  const libRs = `use wclap::prelude::*;

#[wclap_plugin]
pub struct ${toPascalCase(name)} {
    gain: f32,
}

impl Default for ${toPascalCase(name)} {
    fn default() -> Self {
        Self { gain: 0.5 }
    }
}

impl Plugin for ${toPascalCase(name)} {
    fn process(&mut self, input: &[&[f32]], output: &mut [&mut [f32]]) {
        for (out_ch, in_ch) in output.iter_mut().zip(input.iter()) {
            for (out_sample, &in_sample) in out_ch.iter_mut().zip(in_ch.iter()) {
                *out_sample = in_sample * self.gain;
            }
        }
    }

    fn set_parameter(&mut self, id: u32, value: f32) {
        match id {
            0 => self.gain = value,
            _ => {}
        }
    }

    fn get_parameter(&self, id: u32) -> f32 {
        match id {
            0 => self.gain,
            _ => 0.0
        }
    }
}
`;

  await fs.ensureDir(path.join(projectDir, 'src'));
  await fs.writeFile(path.join(projectDir, 'Cargo.toml'), cargoToml);
  await fs.writeFile(path.join(projectDir, 'src', 'lib.rs'), libRs);
}

async function createCppDsp(projectDir: string, name: string) {
  const cmakeLists = `cmake_minimum_required(VERSION 3.21)
project(${name} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# Find wasi-sdk
if(NOT DEFINED WASI_SDK_PREFIX)
  set(WASI_SDK_PREFIX "$ENV{WASI_SDK_PREFIX}")
endif()

add_executable(dsp src/dsp.cpp)

set_target_properties(dsp PROPERTIES
  SUFFIX ".wasm"
  LINK_FLAGS "-Wl,--export-all -Wl,--no-entry"
)
`;

  const dspCpp = `#include <cstdint>

// Plugin state
static float g_gain = 0.5f;
static float g_input[2][4096];
static float g_output[2][4096];

extern "C" {

void dsp_init(float sample_rate, int32_t max_block_size) {
    // Initialize plugin
}

void dsp_process(int32_t block_size) {
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < block_size; ++i) {
            g_output[ch][i] = g_input[ch][i] * g_gain;
        }
    }
}

void dsp_set_param(int32_t id, float value) {
    if (id == 0) g_gain = value;
}

float dsp_get_param(int32_t id) {
    if (id == 0) return g_gain;
    return 0.0f;
}

float* dsp_get_input_buffer(int32_t channel) {
    return g_input[channel];
}

float* dsp_get_output_buffer(int32_t channel) {
    return g_output[channel];
}

} // extern "C"
`;

  await fs.ensureDir(path.join(projectDir, 'src'));
  await fs.writeFile(path.join(projectDir, 'CMakeLists.txt'), cmakeLists);
  await fs.writeFile(path.join(projectDir, 'src', 'dsp.cpp'), dspCpp);
}

async function createAsDsp(projectDir: string, name: string) {
  const packageJson = {
    name: name.toLowerCase(),
    scripts: {
      'asbuild': 'asc assembly/index.ts --target release'
    },
    devDependencies: {
      'assemblyscript': '^0.27.0'
    }
  };

  const indexTs = `// ${name} - AssemblyScript DSP

let gain: f32 = 0.5;

const INPUT_L = memory.data(4096 * 4);
const INPUT_R = memory.data(4096 * 4);
const OUTPUT_L = memory.data(4096 * 4);
const OUTPUT_R = memory.data(4096 * 4);

export function dsp_init(sampleRate: f32, maxBlockSize: i32): void {
  // Initialize
}

export function dsp_process(blockSize: i32): void {
  for (let i = 0; i < blockSize; i++) {
    const inL = load<f32>(INPUT_L + i * 4);
    const inR = load<f32>(INPUT_R + i * 4);
    store<f32>(OUTPUT_L + i * 4, inL * gain);
    store<f32>(OUTPUT_R + i * 4, inR * gain);
  }
}

export function dsp_set_param(id: i32, value: f32): void {
  if (id == 0) gain = value;
}

export function dsp_get_param(id: i32): f32 {
  if (id == 0) return gain;
  return 0.0;
}

export function dsp_get_input_buffer(channel: i32): usize {
  return channel == 0 ? INPUT_L : INPUT_R;
}

export function dsp_get_output_buffer(channel: i32): usize {
  return channel == 0 ? OUTPUT_L : OUTPUT_R;
}
`;

  await fs.ensureDir(path.join(projectDir, 'assembly'));
  await fs.writeJson(path.join(projectDir, 'package.json'), packageJson, { spaces: 2 });
  await fs.writeFile(path.join(projectDir, 'assembly', 'index.ts'), indexTs);
}

async function createUi(projectDir: string, name: string, template: string) {
  await fs.ensureDir(path.join(projectDir, 'ui'));

  const indexHtml = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>${name}</title>
  <script src="clasp.js"></script>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #1a1a2e;
      color: #eee;
      height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .plugin {
      text-align: center;
      padding: 2rem;
    }
    h1 { margin-bottom: 2rem; font-weight: 300; }
    .knob-container { margin: 1rem 0; }
    input[type="range"] {
      width: 200px;
      accent-color: #0ff;
    }
    .value { font-size: 1.5rem; color: #0ff; margin-top: 0.5rem; }
  </style>
</head>
<body>
  <div class="plugin">
    <h1>${name}</h1>
    <div class="knob-container">
      <label>Gain</label>
      <input type="range" id="gain" min="0" max="1" step="0.01" value="0.5">
      <div class="value" id="gain-value">50%</div>
    </div>
  </div>

  <script>
    const gainSlider = document.getElementById('gain');
    const gainValue = document.getElementById('gain-value');

    // Update display
    function updateGainDisplay(value) {
      gainValue.textContent = Math.round(value * 100) + '%';
      gainSlider.value = value;
    }

    // User input
    gainSlider.addEventListener('input', (e) => {
      const value = parseFloat(e.target.value);
      updateGainDisplay(value);
      clasp.call('setParam', 0, value);
    });

    // Receive updates from plugin
    clasp.on('param', (data) => {
      if (data.id === 0) {
        updateGainDisplay(data.v);
      }
    });

    // Initialize
    clasp.on('ready', async () => {
      const info = await clasp.call('getPluginInfo');
      console.log('Plugin loaded:', info);
    });
  </script>
</body>
</html>
`;

  await fs.writeFile(path.join(projectDir, 'ui', 'index.html'), indexHtml);

  // Copy clasp.js from clasp-gui (placeholder - would need actual path resolution)
  const claspJs = `// clasp.js - Plugin communication library
// This file should be copied from clasp-gui/js/clasp.js
console.warn('clasp.js placeholder - copy from clasp-gui package');

window.clasp = {
  on: (event, handler) => { console.log('clasp.on:', event); },
  call: async (fn, ...args) => { console.log('clasp.call:', fn, args); return null; },
  send: (type, payload) => { console.log('clasp.send:', type, payload); }
};
`;
  await fs.writeFile(path.join(projectDir, 'ui', 'clasp.js'), claspJs);
}

function getBuildInstructions(lang: string, short = false): string {
  const instructions: Record<string, string> = {
    rust: short
      ? '  cargo build --target wasm32-wasi --release'
      : '```bash\ncargo build --target wasm32-wasi --release\ncp target/wasm32-wasi/release/*.wasm .\n```',
    cpp: short
      ? '  cmake -B build && cmake --build build'
      : '```bash\ncmake -B build -DCMAKE_TOOLCHAIN_FILE=$WASI_SDK_PREFIX/share/cmake/wasi-sdk.cmake\ncmake --build build\n```',
    assemblyscript: short
      ? '  npm install && npm run asbuild'
      : '```bash\nnpm install\nnpm run asbuild\n```'
  };
  return instructions[lang] || '';
}

function toPascalCase(str: string): string {
  return str
    .replace(/[^a-zA-Z0-9]+/g, ' ')
    .split(' ')
    .map(word => word.charAt(0).toUpperCase() + word.slice(1).toLowerCase())
    .join('');
}
