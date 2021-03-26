BEGIN TRANSACTION;
PRAGMA user_version = 2;
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
CREATE TABLE audio (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE,
  duration INTEGER,
  FOREIGN KEY(file_id) REFERENCES files(id)
);
CREATE TABLE image (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE,
  width INTEGER,
  height INTEGER,
  FOREIGN KEY(file_id) REFERENCES files(id)
);
CREATE TABLE video (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE,
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
INSERT INTO users VALUES(1,'SMS',NULL,NULL,1);
INSERT INTO users VALUES(2,'MMS',NULL,NULL,1);
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
  UNIQUE (uid, thread_id, body, time)
);
ALTER TABLE threads ADD COLUMN last_read_id INTEGER REFERENCES messages(id);

INSERT INTO users VALUES(3,'New Person','New Person',NULL,1);
INSERT INTO users VALUES(4,'+19876543210',NULL,NULL,1);
INSERT INTO users VALUES(5,'+19812121212',NULL,NULL,1);
INSERT INTO users VALUES(6,'Random Person','Random Person',NULL,1);
INSERT INTO users VALUES(7,'Bob','Bob',NULL,1);

INSERT INTO accounts VALUES(3,4,NULL,0,5);
INSERT INTO accounts VALUES(4,5,NULL,0,5);

INSERT INTO threads VALUES(1,'Random room','Random room',NULL,4,1,0,NULL);
INSERT INTO threads VALUES(2,'Random room','Random room',NULL,3,1,0,NULL);
INSERT INTO threads VALUES(3,'Another Room@example.com','Another Room@example.com',NULL,3,1,0,NULL);

INSERT INTO thread_members VALUES(1,1,3);
INSERT INTO thread_members VALUES(2,2,3);
INSERT INTO thread_members VALUES(3,2,6);
INSERT INTO thread_members VALUES(4,3,6);
INSERT INTO thread_members VALUES(5,3,7);
INSERT INTO thread_members VALUES(6,1,6);

INSERT INTO messages VALUES(NULL,'3f5f7d60-1510-4249-80f4-ad802fa9483f',1,NULL,NULL,'Hello',2,1,1502695426,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'c485ac17-513e-4e16-b049-dbc21e000ed8',1,NULL,NULL,'Hi',2,1,1502695424,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'26b5bd41-8f34-476a-bb03-9ed8f8129817',1,3,NULL,'I''m New, Hi',2,1,1502695429,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'4d3defa2-85a2-4cd5-9e1b-940b2c406351',2,3,NULL,'New here',2,1,1502695429,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'955044fb-fc34-42a1-88c7-acdd0c45acc7',2,6,NULL,'I''m random',2,1,1502695432,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'1525c407-7c3d-4b02-8e26-a6e86183a8bc',3,4,NULL,'Hello all',2,-1,1502695573,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'be6ca8bf-b5d9-4983-bbd3-3767eda52f4a',3,NULL,NULL,'I''m empty',2,1,1502695572,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'53269985-89da-4e01-9914-fa053735d59f',3,6,NULL,'Another me',2,1,1502695432,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'92a4e961-b3ac-487c-9dd6-c645944e5946',3,7,NULL,'I''m bob',2,1,1502695569,NULL,0,NULL);
INSERT INTO messages VALUES(NULL,'21fb7985-c3c4-4292-ab84-1b7c637c727a',1,6,NULL,'Let me know who is here?',2,1,1502695587,NULL,0,NULL);

COMMIT;
