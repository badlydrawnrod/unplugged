Feature: Supported document sizes
  Document byte lengths are limited so byte offsets and logical line counts
  remain representable. Oversized input is rejected in every build mode.

  Scenario: Oversized files cannot be loaded for editing
    Given a file whose byte length exceeds the supported document-size limit
    When the file is loaded for editing
    Then loading fails with a size-limit error
