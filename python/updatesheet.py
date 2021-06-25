#!/usr/bin/env python3

import dbaccess, gsheet

import argparse
from datetime import date


def append_orders(sheet_api, database):
    """ returns ID of last appended Order """
    first_column = sheet_api.read("A:A")
    try:
        previous_id = int(first_column[-1][0] if first_column else 0)
    except ValueError:
        previous_id = 0
    print("Previous order id {}.".format(previous_id))

    new_orders = database.get_orders_idrange(previous_id)
    if new_orders:
        sheet_api.append("Orders", new_orders)
        return (previous_id, new_orders[-1][0]) # Last row, first column -> the ID of last appended order
    else:
        return None


def append_fragments(sheet_api, database, orders_range):
    sheet_api.append("Fragments", database.get_fragments(orders_range[0], orders_range[1]))


def main():
    parser = argparse.ArgumentParser("Write missing content from dogebot SQLite database to Google Sheet.")
    parser.add_argument("sqlitefile", help="SQLite3 database file.")
    parser.add_argument("tokenfile", help="The file containing user access tokens.")
    parser.add_argument("spreadsheet", help="The ID of the Google spreadhseet.")
    args = parser.parse_args()

    database = dbaccess.Database(args.sqlitefile)
    sheet_api = gsheet.Spreadsheet(args.tokenfile, args.spreadsheet)
    orders_range = append_orders(sheet_api, database)
    if orders_range:
        append_fragments(sheet_api, database, orders_range)


if __name__ == "__main__":
    main()
