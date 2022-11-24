
//REV: basic loop variable that will be passed. It contains:
// (atomic) bool (loop) -- specifies continue looping if true
// condition_variable and mutex, which must be used as an additional
// condition (with || OR) for all sleeps, waits, etc.

// Single atomic for loop variable is good, but it does not address the issue
// where a producer dies, and a consumer waits infinitely for some condition
// based on it. It may be in some inner loop e.g. while(not condition)

// This is "fixed" by forcing user to include this in all waits/sleeps
