#!/usr/bin/env python3

import cgi
import sqlite3
import os
import hashlib
import json

# Path to the database
DATABASE_PATH = 'var/www/users.db'

# Create a connection to the database and initialize if necessary
conn = sqlite3.connect(DATABASE_PATH)
c = conn.cursor()

# Create table if it doesn't exist
c.execute('''
CREATE TABLE IF NOT EXISTS users (
    username TEXT PRIMARY KEY,
    password_hash TEXT NOT NULL
)
''')
conn.commit()
conn.close()

def hash_password(password):
    """Hash the password with SHA-256 for secure storage."""
    return hashlib.sha256(password.encode()).hexdigest()

def handle_request():
    form = cgi.FieldStorage()
    action = form.getvalue('action')
    username = form.getvalue('username')
    password = form.getvalue('password')

    if not username or not password:
        print(json.dumps({"success": False, "message": "Nom d'utilisateur ou mot de passe manquant"}))
        return

    conn = sqlite3.connect(DATABASE_PATH)
    c = conn.cursor()

    if action == 'register':
        try:
            c.execute('INSERT INTO users (username, password_hash) VALUES (?, ?)', (username, hash_password(password)))
            conn.commit()
            print(json.dumps({"success": True, "message": "Inscription réussie"}))
        except sqlite3.IntegrityError:
            print(json.dumps({"success": False, "message": "Le nom d'utilisateur existe déjà"}))

    elif action == 'login':
        c.execute('SELECT password_hash FROM users WHERE username = ?', (username,))
        row = c.fetchone()

        if row and row[0] == hash_password(password):
            print(json.dumps({"success": True, "message": "Connexion réussie"}))
        else:
            print(json.dumps({"success": False, "message": "Identifiants incorrects"}))

    conn.close()

if __name__ == "__main__":
    handle_request()
