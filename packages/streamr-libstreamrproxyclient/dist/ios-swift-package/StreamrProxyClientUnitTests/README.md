# StreamrProxyClientUnitTests

A test suite for validating the functionality of the StreamrProxyClient API using mocked components.

## Overview

This package contains unit tests for the StreamrProxyClient API, focusing on testing the client's behavior in isolation using mock implementations. The tests cover various scenarios including initialization, connection handling, and message publishing.

## Requirements

- Swift 5.9 or later
- macOS 13.0 or later

## Running Tests

- To run the test suite, navigate to the package directory and execute in Terminal: swift test

## Test Coverage

The test suite includes verification of:

- Client initialization with valid and invalid parameters
- Client deinitialization
- Connection handling with various proxy configurations
- Message publishing
- Error handling for:
  - Invalid Ethereum addresses
  - Invalid stream part IDs
  - Invalid proxy URLs
  - Empty proxy lists

## Project Structure

- `Tests/StreamrProxyClientUnitTests_MockedTest/`
  - `MockedProxyClientTests.swift` - Main test suite using mocked components
  - Mock implementations of the ProxyClientAPI for testing

## Dependencies

- `@testable import ProxyClientAPI` - The main API being tested
- `Testing` - Swift testing framework

## Test Constants

The test suite uses a set of predefined constants for testing, including:
- Mock WebSocket URLs
- Valid and invalid Ethereum addresses
- Stream part IDs
- Test messages
