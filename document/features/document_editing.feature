Feature: Document byte editing
  Edits replace a byte range and keep logical line queries consistent with
  the resulting content. Byte offsets and line numbers are zero-based.
  In quoted content, \n represents a newline byte.

  Scenario: Inserting text and a newline preserves surrounding content
    Given a document containing "ab\ncd\ne"
    When "xy\n" is inserted at byte 2
    Then the document contains "abxy\n\ncd\ne"
    And its four logical lines start at bytes 0, 5, 6, and 9

  Scenario: Deleting a range can remove text and a newline together
    Given a document containing "ab\ncd\ne"
    When three bytes are deleted at byte 2
    Then the document contains "ab\ne"
    And its two logical lines start at bytes 0 and 3

  Scenario: Deleting a newline joins adjacent lines
    Given a document containing "ab\ncd\ne"
    When the newline at byte 2 is deleted
    Then the document contains "abcd\ne"
    And its two logical lines start at bytes 0 and 5

  Scenario: Equal-length replacement changes logical line boundaries
    Given a document containing "ab\ncd\ne"
    When three bytes at byte 2 are replaced with "xy\n"
    Then the document contains "abxy\n\ne"
    And its three logical lines start at bytes 0, 5, and 6

  Scenario: A longer replacement preserves the suffix
    Given a document containing "ab\ncd\ne"
    When the newline at byte 2 is replaced with "xy\n"
    Then the document contains "abxy\ncd\ne"
    And its three logical lines start at bytes 0, 5, and 8

  Scenario: A shorter replacement can span multiple lines
    Given a document containing "ab\ncd\ne"
    When five bytes at byte 1 are replaced with "X\n"
    Then the document contains "aX\ne"
    And its two logical lines start at bytes 0 and 3
    And the end of the document belongs to line 1

  Scenario: An empty edit preserves content and line boundaries
    Given a document containing "ab\ncd\ne"
    When edits at bytes 0, 3, and 2 delete and insert no bytes
    Then the document still contains "ab\ncd\ne"
    And its three logical lines still start at bytes 0, 3, and 6

  Scenario: Deleting one adjacent newline preserves the other
    Given a document containing "A\nB"
    When a newline is inserted at byte 2
    Then its three logical lines start at bytes 0, 2, and 3
    When the newline at byte 2 is deleted
    Then the document contains "A\nB"
    And its two logical lines start at bytes 0 and 2

  Scenario: Appending a newline creates an empty final line
    Given a document containing "ab"
    When a newline is appended
    Then the document contains "ab\n"
    And its two logical lines start at bytes 0 and 3
    And the final logical line is empty
    And the end of the document belongs to line 1
