Feature: Document edit failures
  An edit either succeeds completely or leaves the document unchanged.

  Scenario: An allocation failure leaves a replacement unapplied
    Given a document containing "a", a newline, "b", a newline, and "c"
    And replacement text contains three newline-terminated letters "x", "y", "z"
    When replacing the middle three bytes fails because memory cannot be allocated
    Then the document still contains its original bytes and three logical lines
    And the same replacement succeeds when memory is available again

  Scenario: Deletion succeeds when allocation is unavailable
    Given a document containing "a", a newline, "b", a newline, and "c"
    And memory allocation is unavailable
    When the middle three bytes are deleted
    Then the document contains "ac" in one logical line
