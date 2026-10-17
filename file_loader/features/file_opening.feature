Feature: File opening outcomes
  The loader distinguishes an empty file from a file it cannot open or read.

  Scenario: An empty file loads successfully
    Given an existing file with no bytes
    When the file is loaded for editing
    Then loading succeeds with an empty byte buffer

  Scenario: A missing file reports a load failure
    Given a path that does not name an existing file
    When the file is loaded for editing
    Then loading reports a file error

  Scenario: A path that cannot be read as a file reports a load failure
    Given a directory path where a file is expected
    When the path is loaded for editing
    Then loading reports a file error
