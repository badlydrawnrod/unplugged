Feature: Wrapped logical lines
  Lines are displayed as rows whose content fits within a positive byte width.

  Scenario: A wide viewport shows a short line in one row
    Given a logical line containing "abc" followed by a newline
    When it is wrapped at the largest supported widths
    Then it produces exactly one row containing "abc"
