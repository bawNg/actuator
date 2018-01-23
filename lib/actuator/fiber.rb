require 'fiber'

class Fiber
  attr_accessor :job, :last_job
  attr_writer :is_root

  current.is_root = true
  current.job = Job.new
  current.job.id = 0

  def root?
    @is_root
  end
end