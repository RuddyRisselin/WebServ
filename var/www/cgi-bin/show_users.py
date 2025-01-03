#!/usr/bin/env python3

import sqlite3

DATABASE_PATH = 'var/www/users.db'

print("Content-Type: text/html\n")

try:
    conn = sqlite3.connect(DATABASE_PATH)
    c = conn.cursor()

    c.execute('SELECT username, password_hash FROM users')

    users = c.fetchall()
    if users:
        print("<table border='1'>")
        print("<tr><th>Nom d'utilisateur</th><th>Mot de passe haché</th></tr>")
        for user in users:
            print(f"<tr><td>{user[0]}</td><td>{user[1]}</td></tr>")
        print("</table>")
    else:
        print("<p>Aucun utilisateur enregistré.</p>")

    conn.close()

except Exception as e:
    print(f"<p>Erreur lors de la récupération des utilisateurs : {str(e)}</p>")
