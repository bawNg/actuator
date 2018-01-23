# -*- ruby -*-

require "rubygems"
require "hoe"
require 'rake/extensiontask'

Hoe.plugin :clean

Hoe.spec "actuator" do
  developer 'bawNg', 'bawng@intoxicated.co.za'
  license 'MIT'
  self.urls = 'https://github.com/bawNg/actuator'

  self.extra_dev_deps << ['rake-compiler', '>= 0']
  self.extra_dev_deps << ['minitest', '>= 0']
  self.spec_extras = { extensions: ['ext/actuator/extconf.rb'] }

  Rake::ExtensionTask.new('actuator', spec) do |ext|
    ext.lib_dir = File.join('lib', 'actuator')
  end
end

Rake::Task[:test].prerequisites << :clean << :compile
