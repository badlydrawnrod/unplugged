Feature: Decode terminal input independently of byte acquisition
  Terminal input bytes and acquisition outcomes determine decoded keys.

  Scenario: UTF-8 input produces one text key per character
    Given the input stream contains UTF-8 for "é€😀x"
    When I decode four keys
    Then the text keys contain "é", "€", "😀", and "x" in order
    And none of the keys has modifiers
    And the end of the stream produces no key

  Scenario: Legacy and kitty control input decode to the same key
    Given one stream contains the legacy byte 0x01
    And another stream contains the kitty sequence ESC [ 97 ; 5 u
    When I decode one key from each stream
    Then both keys contain lowercase "a" with only the Ctrl modifier

  Scenario: Escape is distinguished by the next byte or a timeout
    Given separate input streams begin with Escape
    When the next outcome is a timeout
    Then the decoded key is Escape with no modifiers
    When the next byte is Escape
    Then the decoded key is Alt+Escape
    When the next byte is lowercase "a"
    Then the decoded key is lowercase "a" with only the Alt modifier
    When the next outcome is EOF or an acquisition error
    Then no key is decoded
