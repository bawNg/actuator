module Actuator
  class Mutex
    def initialize
      @waiters = []
    end

    def lock
      job = Job.current
      raise FiberError if @waiters.include? job
      @waiters << job
      if @waiters.size > 1
        Job.yield
      end
      true
    end

    def locked?
      !@waiters.empty?
    end

    def _wake_up(job)
      if job.sleep_timer
        job.wake!
      elsif job.mutex_asleep
        job.mutex_asleep = nil
        job.fiber.resume
      else
        raise FiberError, "_wake_up called for job #{job.id} which is #{job.state}"
      end
    end

    def sleep(timeout=nil)
      unlock
      if timeout
        Job.sleep(timeout)
      else
        job = Job.current
        job.mutex_asleep = self
        begin
          Job.yield
        ensure
          job.mutex_asleep = nil
        end
      end
      nil
    ensure
      lock
    end

    def try_lock
      lock unless locked?
    end

    def unlock
      unless @waiters.first == Job.current
        raise "[Job #{Job.current.id}] Mutex#unlock called from job which does not have the lock - @waiters: #{@waiters.map(&:id).inspect}"
      end
      @waiters.shift
      unless @waiters.empty?
        Actuator.next_tick do
          next if @waiters.empty?
          @waiters.first.fiber.resume
        end
      end
      self
    end

    def synchronize
      lock
      yield
    ensure
      unlock
    end
  end

  class ConditionVariable
    def initialize
      @waiters = []
    end

    def wait(mutex, timeout=nil)
      job = Job.current
      waiter = [mutex, job]
      @waiters << waiter
      mutex.sleep timeout
      self
    ensure
      @waiters.delete waiter
    end

    def signal
      while waiter = @waiters.shift
        job = waiter[1]
        next unless job.alive?
        Actuator.next_tick do
          waiter[0]._wake_up(job) if job.alive?
        end
        break
      end
      self
    end

    def broadcast
      waiters = @waiters
      @waiters = []
      waiters.each do |waiter|
        job = waiter[1]
        next unless job.alive?
        Actuator.next_tick do
          waiter[0]._wake_up(job) if job.alive?
        end
      end
      self
    end
  end
end