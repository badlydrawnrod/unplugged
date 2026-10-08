Feature: Empty documents
  Every document contains at least one logical line, starting at byte zero.
  These scenarios apply to both default and empty-vector construction.

  Scenario: An empty document contains one empty logical line
    Given a newly constructed empty document
    Then its byte length is zero
    And it contains one empty logical line starting at byte zero
    And byte position zero belongs to line zero

  Scenario: An empty edit preserves the empty logical line
    Given a newly constructed empty document
    When an edit at byte zero deletes zero bytes and inserts no bytes
    Then the document contains one empty logical line starting at byte zero

  Scenario: Text and newlines can be inserted into an empty document
    Given a newly constructed empty document
    When "a" is inserted at byte zero
    Then the document contains one logical line starting at byte zero
    When two newline bytes are appended
    Then the document contains three logical lines starting at bytes 0, 2, and 3
    And the last logical line is empty

  Scenario: Deleting all content leaves an editable empty logical line
    Given an empty document filled with "a", two newlines, "b", and a newline
    When all bytes are deleted
    Then the document contains one empty logical line starting at byte zero
    When a newline is inserted at byte zero
    Then the document contains two logical lines starting at bytes 0 and 1
    When that newline is deleted
    Then the document contains one empty logical line starting at byte zero
