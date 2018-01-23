require_relative "setup_test"

module Actuator
  class TestActuator < Test
    PrecisionTestDuration = 10.0
    PrecisionTestInterval = 0.002
    PrecisionTotalSamples = (PrecisionTestDuration / PrecisionTestInterval).to_i

    def setup
      # If GC desyncs the assert thread too much it will cause false positives
      GC.start full_mark: true, immediate_sweep: true
    end

    def test_once_off_timer
      called = called2 = 0
      timer = Timer.in(0.1) { called += 1 }
      timer2 = Timer.in(0.1) { called2 += 1 }
      assert_async do
        timer2.destroy
        assert timer2.destroyed?, 'destroyed? returned false after calling Timer#destroy'
        Kernel.sleep 0.02
        assert !timer.destroyed?, 'timer destroyed too early'
        Kernel.sleep 0.1
        assert timer.destroyed?, 'timer not destroyed within 20ms'
        Kernel.sleep 0.1
        assert called == 1, 'timer callback not called'
        assert called2 == 0, 'timer callback called after being destroyed'
      end
    end

    def test_interval_timer
      called = called2 = 0
      timer = Timer.every(0.005) { called += 1 }
      timer2 = Timer.every(0.005) { called2 += 1; timer2.destroy }
      assert_async do
        Kernel.sleep 0.1
        assert called > 10, "5ms interval timer only fired #{called} times in 100ms"
        timer.destroy
        total_called = called
        Kernel.sleep 0.02
        assert called == total_called, 'interval timer fired after being destroyed'
        assert called2 > 0, '5ms interval timer never called'
        assert called2 == 1, 'Timer#destroy did not work in interval callback'
      end
    end

    #TODO: Implement sampling in the C++ extension to eliminate profiling overhead
    def test_timer_precision
      fiber = Fiber.current
      timer = nil
      lates = Array.new(PrecisionTotalSamples)
      # Try skip initial warmup slowdowns so that they aren't included in the samples
      Timer.in(0.5) do
        attempts = 0
        i = -1
        scheduled_at = Actuator.now
        timer = Timer.every(PrecisionTestInterval) do
          now = Actuator.now
          lates[i += 1] = (now - scheduled_at - PrecisionTestInterval) * 1_000_000
          scheduled_at = Actuator.now
          next if i < PrecisionTotalSamples - 1
          lates.sort!
          below_1000_count = 0
          sum = 0.0; lates.each {|late| below_1000_count += 1 if late < 1000; sum += late }
          mean = sum / lates.size
          median = lates.size % 2 == 0 ? (lates[lates.size/2] + lates[lates.size/2-1]) / 2.0 : lates[(lates.size-1)/2]
          sum = 0.0; lates.each {|late| sum += (late - mean) ** 2 }
          std_deviation = Math.sqrt(sum / (lates.size - 1).to_f)
          Log.puts "[Timer Precision] Total: #{lates.size}, Over 1ms: #{lates.size - below_1000_count}, Low: #{lates.first.round(1)} us, High: #{lates.last.round(1)} us, Median: #{median.round(1)} us, Variance: #{std_deviation.round(1)} us"
          begin
            # Most hardware will provide good precision but this should be able to pass on a Raspberry Pi v1 or a VM
            assert lates.last < 10000, "a timer was over 10 ms late (#{lates.last.round(2)} us)"
            assert median < 200, "average timer precision is worse than 200 us (#{median.round(2)} us)"
            assert std_deviation < 500, "too much jitter, variance is over 500 us (#{std_deviation.round(2)} us)"
          rescue Minitest::Assertion => ex
            Log.puts "Retrying due to bad precision most likely caused by loaded hardware: #{ex.message}"
            attempts += 1
            raise if attempts > 9
            i = -1
            scheduled_at = Actuator.now
            next
          end
          timer.destroy
          fiber.resume
        end
      end
      Fiber.yield
    end
  end
end