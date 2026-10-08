Feature: Ordered byte acquisition
  Byte sources preserve bytes and distinguish temporary absence from ended input.

  Scenario: Finite input preserves every byte before EOF
    Given finite input contains every byte value from 0 through 255 in order
    When I acquire all bytes and the next outcome
    Then every byte is returned unchanged in order
    And the next outcome is EOF

  Scenario: Input can continue after a timeout
    Given no byte is available for an acquisition
    When acquisition reports Timeout
    And bytes become available
    Then subsequent acquisition returns those bytes unchanged in order
