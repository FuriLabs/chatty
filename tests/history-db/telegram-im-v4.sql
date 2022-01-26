BEGIN TRANSACTION;
PRAGMA user_version = 4;
PRAGMA foreign_keys = ON;
CREATE TABLE mime_type (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE
);
CREATE TABLE files (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name TEXT,
  url TEXT NOT NULL UNIQUE,
  path TEXT,
  mime_type_id INTEGER REFERENCES mime_type(id),
  status INT,
  size INTEGER
);
CREATE TABLE file_metadata (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE REFERENCES files(id) ON DELETE CASCADE,
  width INTEGER,
  height INTEGER,
  duration INTEGER,
  FOREIGN KEY(file_id) REFERENCES files(id)
);
CREATE TABLE users (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL,
  alias TEXT,
  avatar_id INTEGER REFERENCES files(id),
  type INTEGER NOT NULL,
  UNIQUE (username, type)
);
INSERT INTO users VALUES(1,'invalid-0000000000000000',NULL,NULL,1);
CREATE TABLE accounts (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER NOT NULL REFERENCES users(id),
  password TEXT,
  enabled INTEGER DEFAULT 0,
  protocol INTEGER NOT NULL,
  UNIQUE (user_id, protocol)
);
INSERT INTO accounts VALUES(1,1,NULL,0,1);
CREATE TABLE threads (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  alias TEXT,
  avatar_id INTEGER REFERENCES files(id),
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
  preview_id INTEGER REFERENCES files(id),
  subject TEXT,
  UNIQUE (uid, thread_id, body, time)
);
CREATE TABLE mm_messages (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  message_id INTEGER NOT NULL UNIQUE REFERENCES messages(id) ON DELETE CASCADE,
  account_id INTEGER NOT NULL REFERENCES accounts(id),
  protocol INTEGER NOT NULL,
  smsc TEXT,
  time_sent INTEGER,
  validity INTEGER,
  reference_number INTEGER
);
CREATE TABLE message_files (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  message_id INTEGER NOT NULL REFERENCES messages(id) ON DELETE CASCADE,
  file_id INTEGER NOT NULL REFERENCES files(id),
  preview_id INTEGER REFERENCES files(id),
  UNIQUE (message_id, file_id)
);
ALTER TABLE threads ADD COLUMN last_read_id INTEGER REFERENCES messages(id);
ALTER TABLE threads ADD COLUMN visibility INT NOT NULL DEFAULT 0;
ALTER TABLE threads ADD COLUMN notification INTEGER NOT NULL DEFAULT 1;

INSERT INTO users VALUES(3,'+19876543210',NULL,NULL,1);
INSERT INTO users VALUES(4,'Alice','Alice',NULL,1);
INSERT INTO users VALUES(5,'Random Person','Random Person',NULL,1);
INSERT INTO users VALUES(6,'+351123456789',NULL,NULL,1);
INSERT INTO users VALUES(7,'Another Person','Another Person',NULL,1);

INSERT INTO accounts VALUES(3,3,NULL,0,5);
INSERT INTO accounts VALUES(4,6,NULL,0,5);

INSERT INTO threads VALUES(1,'Alice','Alice',NULL,3,0,0,NULL,0,1);
INSERT INTO threads VALUES(2,'Random Person','Random Person',NULL,3,0,0,NULL,0,1);
INSERT INTO threads VALUES(3,'Random Person','Random Person',NULL,4,0,0,NULL,0,1);
INSERT INTO threads VALUES(4,'Another Person','Another Person',NULL,4,0,0,NULL,0,1);

INSERT INTO thread_members VALUES(1,1,4);
INSERT INTO thread_members VALUES(2,2,5);
INSERT INTO thread_members VALUES(3,3,5);
INSERT INTO thread_members VALUES(4,4,7);

INSERT INTO messages VALUES(NULL,'a88e7db7-3d41-4e3e-8e21-d1e4e6466a01',1,4,NULL,'How are you',2,1,1502685304,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'84406650-c4a6-435d-ba4f-ac193b59a975',1,4,NULL,'Hi',2,1,1502685300,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'e9d54317-9234-4de8-b345-c3a8e4d3b322',1,4,NULL,'Hello',2,-1,1502685303,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'bf5b5a8c-e9bc-4c22-b215-bdb624c0524d',2,5,NULL,'Hello Random',2,-1,1502685403,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'01241679-58e4-4e65-b88f-67e70d617594',3,5,NULL,'Hi',2,1,1502685271,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'8a7ba154-9e09-4845-973e-cc6f8aedcdc5',3,5,NULL,'Hello',2,1,1502685274,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'b23a7a25-7bdf-44ac-8685-d6881f3eaf90',3,5,NULL,'Yeah',2,-1,1502685280,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'0887db8b-11f1-4167-9dfa-c8a4a0fad6d2',3,5,NULL,'Can you call me @9:00?',2,1,1502685295,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'5a60ea9e-e6a0-4c5e-94bf-5e2330be4547',4,7,NULL,'Hi',2,-1,1502685282,NULL,0,NULL,NULL);
INSERT INTO messages VALUES(NULL,'dd12cdf6-0d8c-4010-8138-9640237ccc15',4,7,NULL,'I''m here',2,1,1502685284,NULL,0,NULL,NULL);

COMMIT;
