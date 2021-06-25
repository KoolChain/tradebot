#!/usr/bin/env python3

import gsheet

import argparse


def main():
    parser = argparse.ArgumentParser("Generate the authorization to access a user Google Sheet.")
    parser.add_argument("credentials", help="The application credentials file.")
    parser.add_argument("output", help="The output file to write user access tokens.")
    args = parser.parse_args()

    gsheet.authorize(args.credentials, args.output)

if __name__ == "__main__":
    main()

