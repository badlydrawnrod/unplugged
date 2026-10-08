Feature: Editor editing commands
  The editor changes document bytes and moves its cursor through key commands.

  Scenario: Insert text and split a line at the cursor
    Given the document contains "ab" and the cursor is after "a"
    When I type "x" and press Enter
    Then the document contains "ax\nb"
    And the cursor is at the start of the second line
    And the frame shows the two numbered lines

  Scenario: Backspace on an empty line joins only the preceding line
    Given the document contains "a\n\nb"
    And the cursor is at the start of the empty second line
    When I press Backspace
    Then the document contains "a\nb"
    And the cursor is after "a"
