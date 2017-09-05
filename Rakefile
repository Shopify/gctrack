task :default => :test

# ==========================================================
# Packaging
# ==========================================================

GEMSPEC = eval(File.read('gctrack.gemspec'))

require 'rubygems/package_task'
Gem::PackageTask.new(GEMSPEC) do |pkg|
end

# ==========================================================
# Ruby Extension
# ==========================================================

require 'rake/extensiontask'
Rake::ExtensionTask.new('gctrack', GEMSPEC) do |ext|
  ext.ext_dir = 'ext/gctrack'
  ext.lib_dir = 'lib/gctrack'
end
task :build => :compile

# ==========================================================
# Testing
# ==========================================================

require 'rake/testtask'
Rake::TestTask.new 'test' do |t|
  t.test_files = FileList['test/test_*.rb']
end
task :test => :build

require "bundler/gem_tasks"
