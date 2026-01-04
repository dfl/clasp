#!/usr/bin/env node

import { Command } from 'commander';
import { create } from './commands/create.js';
import { init } from './commands/init.js';

const program = new Command();

program
  .name('clasp-create')
  .description('CLI for creating WCLAP audio plugins')
  .version('1.0.0');

program
  .command('create <name>')
  .description('Create a new WCLAP plugin project')
  .option('-t, --type <type>', 'Plugin type (effect, instrument)', 'effect')
  .option('-l, --lang <language>', 'DSP language (cpp, rust)', 'cpp')
  .option('-v, --vendor <vendor>', 'Vendor name', 'MyCompany')
  .action(create);

program
  .command('init')
  .description('Initialize a WCLAP project in the current directory')
  .action(init);

// Default command - create without subcommand
program
  .argument('[name]', 'Plugin name')
  .option('-t, --type <type>', 'Plugin type (effect, instrument)', 'effect')
  .option('-l, --lang <language>', 'DSP language (cpp, rust)', 'cpp')
  .option('-v, --vendor <vendor>', 'Vendor name', 'MyCompany')
  .action((name, options) => {
    if (name) {
      create(name, options);
    } else {
      program.help();
    }
  });

program.parse();
