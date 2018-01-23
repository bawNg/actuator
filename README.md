## Welcome to Actuator

Code: https://github.com/bawNg/actuator \
Bugs: https://github.com/bawNg/actuator/issues

Actuator provides high precision scheduling of light weight timers for async and fibered real-time Ruby applications.
Even on Windows where kernel precision is lower, average timer accuracy is ~2 us on modern high end hardware (subject to system load). 

This C++ Ruby extension allows time-sensitive applications to replace native threads with jobs that run in pooled fibers.

This is an alpha release, not recommended for production use. While fairly well tested on Windows and Linux, minimal safety 
and error handling has been implemented in order to minimize overhead. Using the API wrong may result in a segfault.

#### Features

* Provides a high precision float representing the current reactor time
* High precision single threaded timer callback scheduling
* Light weight jobs can be used to replace threads with pooled fibers
* Job-based implementation of sleep, join, kill, Mutex and ConditionVariable
* Job-aware sample-based CPU profiling API and execution time warnings
* Warnings for timers that fire later than the configured threshold
* Low overhead timestamped logging API which is thread-safe

#### Supported platforms

* MRI Ruby 2.x (1.9 is probably compatible but has not been tested).
* High precision timer support is implemented for Windows, Linux and OSX.

#### Getting started

  Install the gem with `gem install actuator` or by adding it to your bundle.

  ```ruby
  require 'actuator'
  
  Actuator.run do
    # Schedule a once off timer which fires after 500 us delay
    Timer.in 0.0005 do
      Log.puts "Reactor time: #{Actuator.now}"
    end
    # Schedule a repeating timer which fires every 50ms
    Timer.every 0.05 do
      Log.puts "50ms have passed"
    end
    # Schedule a timer which we will cancel before it expires
    timer = Timer.in 0.005 do
      Log.warn "This should never be printed"
    end
    # Create a new job which executes inside a pooled fiber
    job1 = Actuator.defer do
      begin
        Log.puts "Job has started"
        # Yield from the job fiber for 200ms
        Job.sleep 0.2
        Log.warn "Job 1 finished sleeping even though it should have been killed"
      ensure
        # The stack is unwound when a job is killed so ensure blocks are executed
        Log.puts "Job 1 is ending"
      end
    end
    # Create another new job
    job2 = Actuator.defer do
      # Yield from job fiber until job1 ends
      job1.join
      Log.puts "Job 2 finished waiting for Job 1"
    end
    Job.sleep 0.001
    # Cancel the 5ms timer that we scheduled above
    timer.destroy
    Job.sleep 0.1
    # Kill job1 before it finishes sleeping
    job1.kill
    # Wait for job2 to end naturally
    job2.join
    Log.puts "Job 2 has ended"
    # Shutdown the reactor
    Actuator.stop
  end
  ```

#### Known issues
- Timer precision is much worse on OSX. This is most likely due to threads taking too long to wake up.
  I don't have an OSX machine to be able to test, hopefully someone else can investigate and submit a patch.
- Memory for active timers will not be freed when calling Actuator.stop
- The profiling API will include time spent yielded from the job.
  The job-aware implementation has been commented out to reduce overhead until the profiling has been rewritten in C++.
- Minimal safety checks and error handling has been implemented in order to minimize overhead. Using the API wrong may result in a segfault.
  Feel free to open a bug report for any such cases that you may come across.


#### Contributing

Actuator is an open source project and any contributions which improve the project are encouraged.
Feel free to make a pull request for any contributions that you would like to see merged into the project.

After cloning the source from this repo, run `rake test` to build the C++ extension and run the reactor and precision tests.

Some ways that you can contribute include:
- Create new [bug reports](https://github.com/bawNg/actuator/issues/new)
- Reviewing and providing detailed feedback on existing [issues](https://github.com/bawNg/actuator/issues/new)
- Writing and improving documentation
- Writing additional tests
- Implementing new features

#### License

Actuator is released under the [MIT License](https://opensource.org/licenses/MIT).