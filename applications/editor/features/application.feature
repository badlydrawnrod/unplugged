Feature: Editor application lifecycle
  The editor opens a file in a terminal session and restores the session on exit.

  Scenario: Opening a file and exiting restores the terminal
    Given an existing file with readable content
    When the editor starts on a pseudo-terminal
    Then the file content is displayed in the terminal
    When Alt+Escape is sent to the running editor
    Then the editor exits successfully
    And the original terminal settings are restored
