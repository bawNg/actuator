require 'minitest'
require 'minitest/autorun'

#require_relative '../lib/actuator'
gem 'actuator'
require 'actuator'

module Minitest
  class << self
    alias_method :run_without_actuator, :run

    def run(args=[])
      result = nil
      Actuator.run do
        Actuator.defer do
          begin
            result = run_without_actuator(args)
          rescue => ex
            Kernel.puts "#{ex.class}: #{ex.message}\n#{ex.backtrace.join "\n"}"
            raise ex
          end
          Actuator.next_tick  { Actuator.stop }
        end
      end
      result
    end
  end
end

module Actuator
  class Test < Minitest::Test
    @@_assert_queue = Queue.new
    @@assert_thread = nil

    # Defer to a thread so that we can sleep reliably without the reactor
    def assert_async
      fiber = Fiber.current
      assert_threaded do
        begin
          yield
        rescue => ex
          Actuator.next_tick { fiber.resume(ex) }
        else
          Actuator.next_tick { fiber.resume }
        end
      end
      result = Fiber.yield
      raise result if result.is_a? Exception
    end

    # Reuse a single thread to reduce the variance of start delay
    def assert_threaded(&block)
      if @@assert_thread
        @@_assert_queue << block
      else
        @@_assert_queue << block
        @@assert_thread = Thread.new do
          @@_assert_queue.pop.() while true
        end
      end
    end
  end
end