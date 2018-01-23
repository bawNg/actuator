require_relative 'actuator/actuator'
require_relative 'actuator/job'
require_relative 'actuator/fiber'
require_relative 'actuator/fiber_pool'

module Actuator
  VERSION = "0.0.3"

  class << self
    def run
      start { defer { yield } if block_given? }
    end

    def next_tick
      Timer.in(0) { yield }
    end

    def defer
      FiberPool.run { yield }
    end
  end
end