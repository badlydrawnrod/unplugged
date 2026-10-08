Feature: Checked terminal output
  Rendering commands are emitted in order and output errors can be handled.

  Scenario: A flushed frame emits cursor movement and row text in order
    Given output is buffered for a terminal endpoint
    When I move to row 2 column 5
    And I write "hello!"
    And I clear the row remainder and move to the next line
    And I flush output
    Then the endpoint receives those commands and text in order

  Scenario: A broken output pipe reports an error instead of terminating
    Given the output endpoint's reader has closed
    When I flush a buffered frame
    Then output reports a broken-pipe system error
    And an empty subsequent flush does not replay the failed frame
    And leaving the output scope restores previous signal handling
