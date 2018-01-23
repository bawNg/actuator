module Actuator
  class JobKilledException < Exception
  end

  JobKilled = JobKilledException.new

  #TODO: Implement Job class in C++ extension so that we can do profiling and extra safety checks with minimal overhead
  class Job
    @@total = 0
    # @@active_jobs = []

    class << self
      # def active
      #   @@active_jobs
      # end

      def current
        Fiber.current.job
      end

      def yield
        job = Job.current
        # if job.time_warning_started_at
        #  duration = Actuator.now - job.time_warning_started_at
        #  job.time_warning_extra ||= 0.0
        #  job.time_warning_extra += duration
        #  Log.puts "[time_warning] suspended: #{job.time_warning_name} (#{(job.time_warning_extra * 1000).round(3)})" if job.time_warning_extra > 0.0025
        # end
        # if resumed_at = job.resumed_at
        #  delta = Actuator.now - resumed_at
        #  if delta > 0.0025
        #    Log.puts "Warning: Job #{job.id} yielding after #{(delta * 1000).round(3)} ms\nJob execution started from #{job.resumed_caller.join "\n"}"
        #  else
        #    #Log.puts "Job #{job.id} yielding - #{(delta * 1000).round(3)} ms"
        #  end
        # else
        #  #Log.puts "Job #{job.id} yielding"
        # end
        job.is_yielded = true
        value = Fiber.yield
        job.is_yielded = false
        if job.ended?
          # job.time_warning_name = nil
          # job.time_warning_started_at = nil
          # job.time_warning_extra = nil
          raise JobKilled
        else
          # now = job.resumed_at = Actuator.now
          # #job.resumed_caller = caller
          # if job.time_warning_started_at
          #  Log.puts "[time_warning] resumed: #{job.time_warning_name} (#{(job.time_warning_extra * 1000).round(3)})" if job.time_warning_extra > 0.0025
          #  job.time_warning_started_at = now
          # else
          #  #Log.puts "Job #{job.id} resumed"
          # end
        end
        value
      end

      def sleep(seconds)
        job = Job.current
        job.sleep_timer = Timer.in(seconds) do
          job.fiber.resume true
        end
        Job.yield
      ensure
        job.sleep_timer = nil
      end

      def wait(jobs, timeout=nil)
        job = Job.current
        jobs << job
        if timeout
          job.sleep_timer = Timer.in(timeout) do
            job.sleep_timer = nil
            job.fiber.resume true
          end
        end
        Job.yield
      end
    end

    attr_accessor :id, :block, :whois, :thread_locals, :system_thread_locals, :fiber, :sleep_timer, :mutex_asleep, :joined_on, :is_yielded, :resumed_at, :resumed_caller, :time_warning_started_at, :time_warning_extra, :time_warning_name
    attr_writer :thread_variables

    def job_started
      raise "[Job #@id] job_started called multiple times" if @id
      @id = @@total += 1
      # @@active_jobs << self
    end

    def job_ended
      @has_ended = true
      @resumed_at = nil
      # @resumed_caller = nil
      # @@active_jobs.delete self
      if defined? @joined_jobs
        joined_jobs = @joined_jobs
        @joined_jobs = nil
        joined_jobs.each(&:resume)
      end
    end

    def yielded?
      @is_yielded
    end

    def asleep?
      @sleep_timer || @mutex_asleep
    end

    def alive?
      !@has_ended
    end

    def ended?
      @has_ended
    end

    def sleep(seconds)
      @sleep_timer = Timer.in(seconds) do
        @sleep_timer = nil
        @fiber.resume true
      end
      Job.yield
    end

    def schedule
      return if @is_scheduled
      @is_scheduled = true
      @sleep_timer.destroy if @sleep_timer
      @sleep_timer = Timer.in(0) do
        @is_scheduled = false
        @sleep_timer = nil
        @fiber.resume
      end
    end

    def wake!
      unless timer = @sleep_timer
        raise "Tried to wake up a job which is not asleep"
      end
      @sleep_timer = nil
      timer.fire!
    end

    def join
      return if @has_ended
      fiber = Fiber.current
      job = fiber.job
      begin
        job.joined_on = self
        (@joined_jobs ||= []) << fiber
        Job.yield
        raise "Job#join - resumed before job #@id ended" unless @has_ended
      ensure
        job.joined_on = nil
        @joined_jobs.delete fiber if @joined_jobs
      end
    end

    def kill
      return if @has_ended
      raise JobKilled if Job.current == self
      if @sleep_timer
        # puts "Kill requested by job #{Job.current.id} (asleep)"
        @sleep_timer.destroy
        @sleep_timer = nil
      elsif @mutex_asleep
        # puts "Kill requested by job #{Job.current.id} (mutex)"
        @mutex_asleep = nil
      elsif @is_yielded
        # puts "Kill requested by job #{Job.current.id} (yielded)"
      else
        raise "[Job #{Job.current.id}] Fiber#kill called on job #{@id} which is #{state}"
      end
      @has_ended = true
      @fiber.resume
    end

    def thread_variable_get(name)
      @thread_variables[name] if @thread_variables
    end

    def thread_variable_set(name, value)
      if @thread_variables
        @thread_variables[name] = value
      else
        @thread_variables = { name => value }
      end
    end

    def state
      if @has_ended;        'ended'
      elsif @sleep_timer;   'asleep'
      elsif @mutex_asleep;  'mutex'
      elsif @joined_on;     "joined on job #{@joined_on.id}"
      elsif @is_yielded;    'yielded'
      elsif !@fiber;        'missing fiber'
      elsif @fiber.alive?;  'alive'
      else                  'dead fiber'
      end
    end

    def puts(msg)
      Log.puts "[Job #@id] #{msg}"
    end

    def time_warning(name=nil)
      raise "Job#time_warning called for job #@id while it is suspended" if yielded?
      if @time_warning_started_at
        duration = Actuator.now - @time_warning_started_at
        @time_warning_extra ||= 0.0
        @time_warning_extra += duration
        (@time_warning_stack ||= []) << [@time_warning_name, @time_warning_extra]
      end
      @time_warning_extra = 0
      if name
        @time_warning_name = name
        @time_warning_started_at = Actuator.now
      else
        @time_warning_name = nil
        @time_warning_started_at = nil
      end
      duration
    end

    def time_warning!(name=nil)
      raise "Job#time_warning! called for job #@id while it is suspended" if yielded?
      if @time_warning_started_at
        now = Actuator.now
        duration = now - @time_warning_started_at + @time_warning_extra
        if duration > 0.0025
          Log.warn "Time warning: #@time_warning_name took #{(duration * 1000).round(3)} ms"
        end
      end
      @time_warning_extra = 0
      if name
        @time_warning_name = name
        @time_warning_started_at = Actuator.now
      else
        if @time_warning_stack && (outstanding = @time_warning_stack.shift)
          @time_warning_name = outstanding[0]
          @time_warning_extra = outstanding[1]
          @time_warning_started_at = Actuator.now
        else
          @time_warning_name = nil
          @time_warning_started_at = nil
        end
      end
      duration
    end
  end
end

Job = Actuator::Job