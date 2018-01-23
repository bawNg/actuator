require_relative '../mutex'

old_verbose, $VERBOSE = $VERBOSE, nil

Mutex = Actuator::Mutex
ConditionVariable = Actuator::ConditionVariable

$VERBOSE = old_verbose