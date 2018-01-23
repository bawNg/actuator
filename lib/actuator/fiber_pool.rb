require 'fiber'

module Actuator
  class FiberPool
    MAX_FIBERS = 10000

    @fibers = []
    @idle_fibers = []
    @queued_jobs = []
    @busy_count = 0

    class << self
      attr_reader :busy_count

      # Always starts fiber immediately - ignores MAX_FIBERS (can cause pool to grow beyond limit)
      def run(whois=nil, &block)
        job = Job.new
        job.block = block
        job.whois = whois

        @busy_count += 1
        fiber = @idle_fibers.pop || create_new_fiber
        fiber.resume(job)

        job
      end

      # Job will be queued if MAX_FIBERS are already active
      def queue(whois=nil, &block)
        job = Job.new
        job.block = block
        job.whois = whois

        if @busy_count >= MAX_FIBERS
          @queued_jobs << job
          #puts "[FiberPool] There are already #@busy_count/#{MAX_FIBERS} busy fibers, queued job #{job.id}"
          return false
        end

        @busy_count += 1
        fiber = @idle_fibers.pop || create_new_fiber
        fiber.resume(job)

        job
      end

      def create_new_fiber
        Fiber.new do |job|
          fiber = Fiber.current
          while true
            #if !job || !job.is_a?(Job)
            #  Log.error "Job #{fiber.last_job ? fiber.last_job.id : -1} fiber resumed after ending and being released to the pool - #{job.inspect}"
            #end
            job.fiber = fiber
            fiber.job = job
            job.job_started
            begin
              job.block.call
            rescue JobKilledException
              # We use a cached exception to avoid the overhead of a catch block since fibers should almost never be killed
            rescue => ex
              Log.error "#{ex.class} while running job: #{ex.message}\n#{ex.backtrace.join "\n"}"
              raise ex
            rescue SystemExit
              break
            end
            job.job_ended
            #fiber.last_job = job
            if @queued_jobs.empty?
              @busy_count -= 1
              fiber.job = nil
              @idle_fibers << fiber
              job = Fiber.yield
            else
              job = @queued_jobs.shift
            end
          end
        end
      end
    end
  end
end