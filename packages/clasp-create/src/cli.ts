#!/usr/bin/env node

import { Command } from 'commander';
import { create } from './commands/create.js';
import { init } from './commands/init.js';

const program = new Command();

program
  .name('clasp')
  .description('CLI for creating and managing WCLAP audio plugins')
  .version('1.0.0');

program
  .command('create <name>')
  .description('Create a new WCLAP plugin project')
  .option('-t, --template <template>', 'Project template (minimal, react, vue)', 'minimal')
  .option('-l, --lang <language>', 'DSP language (rust, cpp, assemblyscript)', 'rust')
  .option('--no-ui', 'Skip UI scaffolding')
  .action(create);

program
  .command('init')
  .description('Initialize a WCLAP project in the current directory')
  .option('-t, --template <template>', 'Project template', 'minimal')
  .action(init);

program.parse();
