BEGIN TRANSACTION;
PRAGMA user_version = 1;
PRAGMA foreign_keys = ON;
CREATE TABLE files (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  url TEXT,
  path TEXT,
  mime_type TEXT,
  size INTEGER
);
CREATE TABLE users (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL,
  alias TEXT, avatar_id INTEGER REFERENCES files(id),
  thumbnail_id INTEGER REFERENCES files(id),
  type INTEGER NOT NULL,
  UNIQUE (username, type)
);
INSERT INTO users VALUES(1,'SMS',NULL,NULL,NULL,1);
INSERT INTO users VALUES(2,'MMS',NULL,NULL,NULL,1);
CREATE TABLE accounts (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER NOT NULL REFERENCES users(id),
  password TEXT,
  enabled INTEGER DEFAULT 0,
  protocol INTEGER NOT NULL,
  UNIQUE (user_id, protocol)
);
INSERT INTO accounts VALUES(1,1,NULL,0,1);
INSERT INTO accounts VALUES(2,2,NULL,0,2);
CREATE TABLE threads (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  alias TEXT,
  account_id INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
  type INTEGER NOT NULL,
  encrypted INTEGER DEFAULT 0,
  UNIQUE (name, account_id, type)
);
CREATE TABLE thread_members (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  thread_id INTEGER NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
  user_id INTEGER NOT NULL REFERENCES users(id),
  UNIQUE (thread_id, user_id)
);
CREATE TABLE messages (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  uid TEXT NOT NULL,
  thread_id INTEGER NOT NULL REFERENCES threads(id) ON DELETE CASCADE,
  sender_id INTEGER REFERENCES users(id),
  user_alias TEXT,
  body TEXT NOT NULL,
  body_type INTEGER NOT NULL,
  direction INTEGER NOT NULL,
  time INTEGER NOT NULL,
  status INTEGER,
  encrypted INTEGER DEFAULT 0,
  UNIQUE (uid, thread_id, body, time)
);
CREATE TABLE media (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE,
  thumbnail_id INTEGER REFERENCES media(id),
  width INTEGER,
  height INTEGER,
  FOREIGN KEY(file_id) REFERENCES files(id)
);
ALTER TABLE threads ADD COLUMN last_read_id INTEGER REFERENCES messages(id);

INSERT INTO users VALUES(3,'charlie@example.org',NULL,NULL,NULL,3);
INSERT INTO users VALUES(4,'room@conference.example.com/bob',NULL,NULL,NULL,3);
INSERT INTO users VALUES(5,'bob@example.com',NULL,NULL,NULL,3);
INSERT INTO users VALUES(6,'alice@example.org',NULL,NULL,NULL,3);
INSERT INTO users VALUES(7,'jhon@example.org',NULL,NULL,NULL,3);

INSERT INTO accounts VALUES(3,3,NULL,0,3);
INSERT INTO accounts VALUES(4,6,NULL,0,3);
INSERT INTO accounts VALUES(5,7,NULL,0,3);

INSERT INTO threads VALUES(1,'another-room@conference.example.com',NULL,5,1,0,NULL);
INSERT INTO threads VALUES(2,'room@conference.example.com',NULL,4,1,0,NULL);
INSERT INTO threads VALUES(3,'room@conference.example.com',NULL,3,1,0,NULL);

INSERT INTO thread_members VALUES(1,2,4);
INSERT INTO thread_members VALUES(2,2,5);
INSERT INTO thread_members VALUES(3,1,5);

INSERT INTO messages VALUES(NULL,'43511f76-0eee-11eb-98fc-23b32f642943',1,7,NULL,'Yes this is another room',2,-1,1587854658,NULL,0);
INSERT INTO messages VALUES(NULL,'12c97d94-0eee-11eb-86e0-7fe0e99a74bb',1,5,NULL,'Is this another room?',2,1,1587854658,NULL,0);
INSERT INTO messages VALUES(NULL,'7f21eca6-0eee-11eb-bdfd-5be4cafcdd69',1,7,NULL,'Feel free to speak anything',2,-1,1587854661,NULL,0);
INSERT INTO messages VALUES(NULL,'1fa48654-0eed-11eb-9110-b7542262f3bf',2,4,NULL,'Hello everyone',2,1,1587854453,NULL,0);
INSERT INTO messages VALUES(NULL,'40a341d8-0eed-11eb-91be-dbcbdfd6fab6',2,4,NULL,'Good morning',2,1,1587854455,NULL,0);
INSERT INTO messages VALUES(NULL,'96001322-0eed-11eb-b943-ffb19c0eb13a',2,5,NULL,'Hi',2,1,1587854458,NULL,0);
INSERT INTO messages VALUES(NULL,'1587644391694312',2,NULL,NULL,'Is this good?',2,1,1587854459,NULL,0);
INSERT INTO messages VALUES(NULL,'d4097d22-0efa-11eb-b349-9317bde881f6',3,3,NULL,'Hello',2,-1,1587854682,NULL,0);

COMMIT;
