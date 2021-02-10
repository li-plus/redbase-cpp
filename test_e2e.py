import os
import random
import re
import sqlite3
import string
import time
from shutil import rmtree

TYPE_INT = 1
TYPE_FLOAT = 2
TYPE_STR = 3


def parse_select(out_msg):
    sep_cnt = 0
    table = []
    lines = out_msg.split('\n')
    lines = [line.strip() for line in lines if line.strip()]

    int_patt = re.compile(r'^[+-]?\d+$')
    float_patt = re.compile(r'^[+-]?\d+\.\d*$')

    for line in lines:
        if line.startswith('+'):
            sep_cnt = (sep_cnt + 1) % 3
            if sep_cnt == 0:
                yield table
                table = []
        elif sep_cnt == 2:
            # ignore leading & trailing empty strings
            row = line.split('|')[1:-1]
            record = []
            for col in row:
                col = col.strip()
                if int_patt.match(col):
                    col = int(col)
                elif float_patt.match(col):
                    col = round(float(col), 2)
                else:
                    col = str(col)
                record.append(col)
            table.append(tuple(record))


def rand_int(max_val=0xffff):
    return random.randint(0, max_val)


def rand_float():
    return random.randint(0, 0xffff) / 100


def rand_str():
    str_len = random.randint(1, 16)
    char_set = string.ascii_lowercase + string.ascii_uppercase
    s = ''.join(random.choices(char_set, k=str_len))
    return f'\'{s}\''


def rand_insert(tab_name, col_types, max_int=0xffff):
    values = []
    for col_type in col_types:
        if col_type == TYPE_INT:
            val = str(rand_int(max_int))
        elif col_type == TYPE_FLOAT:
            val = str(rand_float())
        elif col_type == TYPE_STR:
            val = rand_str()
        else:
            assert False
        values.append(val)
    values = ', '.join(values)
    sql = f'insert into {tab_name} values ({values});'
    return sql


def rand_eq():
    col = random.choice(['a', 'b', 'c'])
    if col == 'a':
        val = rand_int()
    elif col == 'b':
        val = rand_float()
    else:
        val = rand_str()
    return f'{col} = {val}'


def rand_delete():
    cond = rand_eq()
    sql = f'delete from tb where {cond};'
    return sql


def rand_update():
    cond = rand_eq()
    set_clause = rand_eq()
    sql = f'update tb set {set_clause} where {cond};'
    return sql


def check_equal(sqls, restart=True):
    with open('in.sql', 'w') as f:
        f.write('\n'.join(sqls))

    redbase_db_name = 'db'
    mock_db_name = 'mock.db'

    if restart:
        # drop db if it already exists
        if os.path.isdir(redbase_db_name):
            rmtree(redbase_db_name)
        if os.path.isfile(mock_db_name):
            os.remove(mock_db_name)

    query_sqls = []

    # run sqlite3
    start = time.time()
    ans = []
    conn = sqlite3.connect(mock_db_name)
    c = conn.cursor()
    for sql in sqls:
        if sql.startswith('create index') or sql.startswith('drop index'):
            continue
        c.execute(sql)
        if sql.startswith('select'):
            query_sqls.append(sql)
            table = c.fetchall()
            ans.append(table)
    conn.commit()
    conn.close()
    print(f'SQLite3 spent {time.time() - start:.4f}s')

    # run redbase
    start = time.time()
    os.system(f'./build/bin/rawcli {redbase_db_name} < in.sql > rb.out 2>&1')
    print(f'Redbase spent {time.time() - start:.4f}s')

    # check output
    with open('rb.out') as f:
        out_msg = f.read()

    out = list(parse_select(out_msg))
    assert len(out) == len(ans) == len(query_sqls)
    for i, (query_sql, out_tb, ans_tb) in enumerate(zip(query_sqls, out, ans)):
        assert len(out_tb) == len(ans_tb)
        out_tb = sorted(out_tb)
        ans_tb = sorted(ans_tb)
        assert out_tb == ans_tb
        print(f'Test #{i:02d}: PASSED {query_sql}')


def test_single():
    # build table
    sqls = ['create table tb (a int, b float, c char(16));',
            'create index tb(a);']

    col_types = [TYPE_INT, TYPE_FLOAT, TYPE_STR]
    for _ in range(10000):
        sql = rand_insert('tb', col_types)
        sqls.append(sql)

    sqls.append('create index tb(b);')

    # random update / delete / insert
    for _ in range(1000):
        choice = random.randint(0, 2)
        if choice == 0:
            sql = rand_insert('tb', col_types)
        elif choice == 1:
            sql = rand_delete()
        elif choice == 2:
            sql = rand_update()
        else:
            assert False
        sqls.append(sql)

    sqls += [
        'select * from tb;',
        'select * from tb where a > 10000;',
        'select * from tb where a <= 20000;',
        'select * from tb where a < 10000 and a > 20000;',
        'select * from tb where b >= 100. and a < 30000;',
        'select * from tb where a <> 100 and a <> 200 and b <> 50.00;',
        'select * from tb where c > \'m\';',
        'select * from tb where c < \'hello world\';',
    ]

    check_equal(sqls)


def test_multi():
    sqls = ['create table tb1 (s int, a int, b float, c char(16));',
            'create index tb1(a);',
            'create table tb2 (x float, y int, z char(32), s int);',
            'create index tb2(y);',
            'create table tb3 (m int, n float);']

    tb1_types = [TYPE_INT, TYPE_INT, TYPE_FLOAT, TYPE_STR]
    for _ in range(100):
        sql = rand_insert('tb1', tb1_types, max_int=100)
        sqls.append(sql)

    tb2_types = [TYPE_FLOAT, TYPE_INT, TYPE_STR, TYPE_INT]
    for _ in range(100):
        sql = rand_insert('tb2', tb2_types, max_int=100)
        sqls.append(sql)

    tb3_types = [TYPE_INT, TYPE_FLOAT]
    for _ in range(10):
        sql = rand_insert('tb3', tb3_types, max_int=100)
        sqls.append(sql)

    # query
    sqls += [
        'select * from tb1;',
        'select * from tb2;',
        'select * from tb3;',
        'select * from tb1, tb2;',
        'select * from tb3, tb2;',
        'select * from tb1, tb2, tb3;',
        'select * from tb1, tb2, tb3 where tb1.s = tb2.s;',
        'select * from tb1, tb2 where a > 40000 and y < 20000;',
        'select * from tb2, tb3 where tb2.s = m;',
    ]

    check_equal(sqls)


def test_index_join():
    sqls = ['create table tb1 (s int, a int, b float, c char(16));',
            'create index tb1(a);',
            'create table tb2 (x float, y int, z char(32), s int);',
            'create index tb2(y);',
            'create table tb3 (m int, n int);',
            'create index tb3(m);']

    tb1_types = [TYPE_INT, TYPE_INT, TYPE_FLOAT, TYPE_STR]
    for _ in range(10000):
        sql = rand_insert('tb1', tb1_types)
        sqls.append(sql)

    tb2_types = [TYPE_FLOAT, TYPE_INT, TYPE_STR, TYPE_INT]
    for _ in range(10000):
        sql = rand_insert('tb2', tb2_types)
        sqls.append(sql)

    tb3_types = [TYPE_INT, TYPE_INT]
    for _ in range(10000):
        sql = rand_insert('tb3', tb3_types)
        sqls.append(sql)

    # query
    sqls += [
        'select * from tb1, tb2 where a = y;',
        'select * from tb1, tb2 where y = a;',
        'select * from tb1, tb2, tb3 where a = y and a = m;',
        'select * from tb1, tb2, tb3 where a = y and m = y;',
    ]

    check_equal(sqls)


def test_basic():
    ddl = [
        "create table tb(s int, a int, b float, c char(16));",
        "create index tb(s);",
        "create table tb2(x int, y float, z char(16), s int);",
        "create table tb3(m int, n int);"
    ]

    dml = [
        # single table
        "select * from tb;",
        "insert into tb values (0, 1, 1., 'abc');",
        "insert into tb values (2, 2, 2., 'def');",
        "insert into tb values (5, 3, 2., 'xyz');",
        "insert into tb values (4, 4, 2., '0123456789abcdef');",
        "insert into tb values (2, 5, -100., 'oops');",
        "insert into tb values (-100, 6, 3., '');",
        "select * from tb;",
        "select * from tb where a = 3;",
        "select * from tb where b > -100.;",
        "select * from tb where a < 2;",
        "select * from tb where b <> 1.;",
        "select * from tb where c = 'abc';",
        "select * from tb where c <= 'def';",
        "select * from tb where c >= 'def';",
        "select * from tb where c >= 'def' and a < 3;",
        "select * from tb where s < a;",
        "select * from tb where a = s;",
        "select * from tb where s > a;",
        "update tb set a = 996 where a = 3;",
        "select * from tb;",
        "update tb set b = 997., c = 'icu' where c = 'xyz';",
        "select * from tb;",
        "delete from tb where a = 996;",
        "select * from tb;",
        "select s from tb;",
        "select a, s from tb;",
        "select a, s, b, c, b, a from tb;",
        # join
        "insert into tb2 values (1, 2., 'abc', 0);",
        "insert into tb2 values (2, 3., 'def', 1);",
        "insert into tb2 values (3, 1., 'ghi', 2);",
        "select * from tb;",
        "select * from tb2;",
        "select * from tb, tb2;",
        "select * from tb2, tb;",
        "insert into tb3 values (1, 11);",
        "insert into tb3 values (3, 33);",
        "insert into tb3 values (5, 55);",
        "select * from tb, tb2, tb3;",
        # join with selector and conditions
        "select * from tb, tb2 where a = x;",
        "select * from tb, tb2 where a = 2 and x = 1;",
        "select * from tb, tb2 where a > x and a > 3 and x <= 2;",
        "select * from tb, tb2 where a <> x and a > 3 and x <= 2;",
        "select * from tb, tb2 where x <= a and a > 3 and x <= 2;",
        "select * from tb, tb2 where tb.s = tb2.s;",
        "select * from tb, tb2, tb3 where tb.s = tb2.s and tb.a = tb3.m;",
        "select * from tb, tb2, tb3 where tb.s = tb3.m and tb2.x = tb3.m;",
        "select * from tb, tb2, tb3 where tb.s = tb2.s and tb2.x = tb3.m;",
        "select * from tb, tb2, tb3 where tb.s = tb2.s and tb2.x = tb3.m \
            and a > 1 and y >= 1.0 and n > 20 and a <> tb.s and x <> tb2.s and m <> n;",
        "select tb.s, y, tb2.s, c from tb, tb2 where tb.s = tb2.s;"
    ]
    check_equal(ddl + dml)

    # test persistent storage
    check_equal(dml, restart=False)


if __name__ == '__main__':
    test_basic()
    test_single()
    test_multi()
    test_index_join()
