Feature: Editor navigation and viewport
  Navigation operates on logical lines and wrapped rows while keeping the
  cursor visible in a viewport with a four-cell line-number gutter.

  Scenario: Home and End distinguish logical lines from the whole document
    Given the document contains "abcdefghij\nk\n"
    And the viewport has four text columns and three rows
    When I press End
    Then the cursor is before the first newline on the third wrapped row
    When I press Home
    Then the cursor is at byte zero
    When I press Ctrl+End
    Then the cursor is at the end of the document on the third screen row
    And the empty trailing line is visible
    When I press Ctrl+Home
    Then the cursor is at byte zero and the viewport starts at the first row

  Scenario: Vertical movement restores the preferred column after a short line
    Given the document contains "abcd\nx\n\nwxyz"
    And all four lines are visible with eight text columns
    And the cursor is at column three of the first line
    When I press Down three times
    Then the cursor is at column three of the fourth line
    When I press Up three times
    Then the cursor is at column three of the first line

  Scenario: Crossing a viewport boundary scrolls one row and selects its start
    Given the document contains "aa\nbb\ncc\ndd"
    And the viewport has two rows
    And the cursor is at column one of the first line
    When I press Down twice
    Then the viewport shows the second and third lines
    And the cursor is at the start of the third line
    When I press Up twice
    Then the viewport starts at the first line and the cursor is at byte zero

  Scenario: Moving across a wrap boundary keeps the cursor visible
    Given the document contains "abcdefghi"
    And the viewport has four text columns and two rows
    When I press Right four times
    Then the cursor is at the start of the second screen row
    When I press Right four more times
    Then the viewport shows "efgh" and "i" as continuation rows
    And the cursor is on the second screen row
    When I press Home
    Then the viewport starts at the first wrapped row

  Scenario: Page navigation overlaps one row and stops at document ends
    Given the document contains "a\nb\nc\nd\ne"
    And the viewport has three rows
    When I press PageDown
    Then the viewport starts at the third line and the cursor is at its start
    When I press PageDown twice
    Then the viewport starts at the fifth line and the cursor is at its start
    When I press PageUp
    Then the viewport starts at the third line and the cursor is at its start
    When I press PageUp twice
    Then the viewport starts at the first line and the cursor is at byte zero
