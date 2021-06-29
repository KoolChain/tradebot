import math, os, sqlite3
from datetime import datetime


def get_timestamp(date):
    # see: https://stackoverflow.com/a/1937636/1027706
    return math.trunc(datetime.combine(date, datetime.min.time()).timestamp() * 1000)


def _generate_orders_select(status_int, previous_id, last_id):
    range_clause = "id > {}".format(previous_id)
    if (last_id):
        range_clause = "{} AND id <= {}".format(range_clause, last_id)
    return "SELECT * FROM Orders WHERE status = {} AND {}".format(status_int, range_clause)


class Database(object):

    def __init__(self, sqlitefile):
        if os.path.exists(sqlitefile):
            self.sqlitefile = sqlitefile
        else:
            raise Exception("Cannot find provided sqlite file: {}.".format(sqlitefile))


    def get_orders_period(self, begin_day, end_day):
        with sqlite3.connect(self.sqlitefile) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM Orders WHERE status = 4 AND fulfill_time >= {} AND fulfill_time < {};"\
                    .format(get_timestamp(begin_day), get_timestamp(end_day)))
            return cursor.fetchall()


    def get_orders_idrange(self, previous_id, last_id = None):
        status_int = 4 # Fulfilled
        with sqlite3.connect(self.sqlitefile) as conn:
            cursor = conn.cursor()
            cursor.execute(_generate_orders_select(status_int, previous_id, last_id))
            return cursor.fetchall()


    def get_fragments(self, previous_order_id, last_order_id):
        status_int = 4 # Fulfilled
        with sqlite3.connect(self.sqlitefile) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT f.* FROM Fragments f INNER JOIN ({}) o "
                           "WHERE f.composed_order = o.id;"
                            .format(_generate_orders_select(status_int,
                                                            previous_order_id,
                                                            last_order_id)))
            return cursor.fetchall()


    def get_balances_after_time(self, previous_time):
        with sqlite3.connect(self.sqlitefile) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM Balances WHERE time > {};"\
                    .format(previous_time))
            return cursor.fetchall()


    def count_launch_period(self, previous_time, last_time):
        """ Count launches that occured during the provided interval (previous_time, last_time] """
        with sqlite3.connect(self.sqlitefile) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT Count() FROM Launches WHERE time > {} AND time <= {};"\
                    .format(previous_time, last_time))
            return (cursor.fetchone()[0])
