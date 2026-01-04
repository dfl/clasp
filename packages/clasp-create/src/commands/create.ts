import fs from 'fs-extra';
import path from 'path';
import chalk from 'chalk';
import ora from 'ora';
import { fileURLToPath } from 'url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

interface CreateOptions {
  type: 'effect' | 'instrument';
  lang: 'cpp' | 'rust';
  vendor: string;
}

export async function create(name: string, options: CreateOptions) {
  const projectDir = path.resolve(process.cwd(), name);
  const snakeName = name.toLowerCase().replace(/[^a-z0-9]/g, '_');
  const pluginId = `com.${options.vendor.toLowerCase().replace(/[^a-z0-9]/g, '')}.${snakeName}`;

  if (await fs.pathExists(projectDir)) {
    console.error(chalk.red(`Error: Directory "${name}" already exists`));
    process.exit(1);
  }

  const spinner = ora(`Creating WCLAP ${options.type} "${name}"...`).start();

  try {
    // Find templates directory (relative to package root)
    const packageRoot = path.resolve(__dirname, '..', '..', '..');
    const templateName = options.type === 'instrument' ? 'synth' : 'gain';
    const templatesDir = path.resolve(packageRoot, '..', '..', 'templates', templateName);

    if (!await fs.pathExists(templatesDir)) {
      throw new Error(`Templates not found at ${templatesDir}`);
    }

    // Create project directory
    await fs.ensureDir(projectDir);

    // Template variables
    const vars: Record<string, string> = {
      '{{PLUGIN_NAME}}': name,
      '{{PLUGIN_NAME_SNAKE}}': snakeName,
      '{{PLUGIN_ID}}': pluginId,
      '{{PLUGIN_VENDOR}}': options.vendor,
      '{{PLUGIN_TYPE}}': options.type,
    };

    // Copy and process templates
    await copyTemplates(templatesDir, projectDir, vars, options.lang);

    spinner.succeed(chalk.green(`Created WCLAP ${options.type}: ${name}`));

    console.log(`
${chalk.bold('Next steps:')}

  ${chalk.cyan(`cd ${name}`)}

${options.lang === 'cpp' ? `  ${chalk.cyan('# Set wasi-sdk path')}
  ${chalk.cyan('export WASI_SDK_PREFIX=/path/to/wasi-sdk')}

  ${chalk.cyan('# Build')}
  ${chalk.cyan('mkdir build && cd build')}
  ${chalk.cyan('cmake ..')}
  ${chalk.cyan('make && make install')}` : `  ${chalk.cyan('# Build')}
  ${chalk.cyan('cargo build --target wasm32-wasip1 --release')}`}

  ${chalk.cyan('# Copy to plugins folder')}
  ${chalk.cyan(`cp -r ${snakeName}.wclap ~/.wclap/plugins/`)}

${chalk.dim('Load thunder.clap in your DAW to test.')}
`);

  } catch (error) {
    spinner.fail(chalk.red('Failed to create project'));
    console.error(error);
    process.exit(1);
  }
}

async function copyTemplates(
  srcDir: string,
  destDir: string,
  vars: Record<string, string>,
  lang: string
) {
  const entries = await fs.readdir(srcDir, { withFileTypes: true });

  for (const entry of entries) {
    const srcPath = path.join(srcDir, entry.name);
    let destName = entry.name.replace('.template', '');

    // Apply variable substitution to filename
    for (const [key, value] of Object.entries(vars)) {
      destName = destName.replace(key, value);
    }

    const destPath = path.join(destDir, destName);

    if (entry.isDirectory()) {
      // Skip cpp if lang is rust, skip rust if lang is cpp
      if ((entry.name === 'cpp' && lang !== 'cpp') ||
          (entry.name === 'rust' && lang !== 'rust')) {
        continue;
      }

      await fs.ensureDir(destPath);
      await copyTemplates(srcPath, destPath, vars, lang);
    } else {
      // Read, substitute, write
      let content = await fs.readFile(srcPath, 'utf-8');

      // Apply variable substitution
      for (const [key, value] of Object.entries(vars)) {
        content = content.replace(new RegExp(key.replace(/[{}]/g, '\\$&'), 'g'), value);
      }

      await fs.writeFile(destPath, content);
    }
  }
}
