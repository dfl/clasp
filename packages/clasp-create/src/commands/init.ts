import chalk from 'chalk';

interface InitOptions {
  template: string;
}

export async function init(options: InitOptions) {
  console.log(chalk.yellow('init command not yet implemented'));
  console.log('Would initialize a WCLAP project in the current directory');
  console.log('Template:', options.template);
}
