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
CREATE TABLE media (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  file_id INTEGER NOT NULL UNIQUE,
  thumbnail_id INTEGER REFERENCES media(id),
  width INTEGER,
  height INTEGER,
  FOREIGN KEY(file_id) REFERENCES files(id)
);
CREATE TABLE users (
  id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL,
  alias TEXT,
  avatar_id INTEGER REFERENCES media(id),
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
  avatar_id INTEGER REFERENCES media(id),
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
ALTER TABLE threads ADD COLUMN last_read_id INTEGER REFERENCES messages(id);

INSERT INTO users VALUES(3,'+12133210011',NULL,NULL,1);
INSERT INTO users VALUES(4,'Mobile@5G',NULL,NULL,1);
INSERT INTO users VALUES(5,'5555',NULL,NULL,1);
INSERT INTO users VALUES(6,'+919876121212',NULL,NULL,1);
INSERT INTO users VALUES(7,'+919995123456',NULL,NULL,1);
INSERT INTO users VALUES(8,'+4915112345678',NULL,NULL,1);

INSERT INTO threads VALUES(1,'+12133210011','+12133210011',NULL,1,0,0,NULL);
INSERT INTO threads VALUES(2,'Mobile@5G','Mobile@5G',NULL,1,0,0,NULL);
INSERT INTO threads VALUES(3,'5555','5555',NULL,1,0,0,NULL);
INSERT INTO threads VALUES(4,'+919876121212','+919876121212',NULL,1,0,0,NULL);
INSERT INTO threads VALUES(5,'+919995123456','+919995123456',NULL,1,0,0,NULL);
INSERT INTO threads VALUES(6,'+4915112345678','01511 2345678',NULL,1,0,0,NULL);

INSERT INTO thread_members VALUES(1,1,3);
INSERT INTO thread_members VALUES(2,2,4);
INSERT INTO thread_members VALUES(3,3,5);
INSERT INTO thread_members VALUES(4,4,6);
INSERT INTO thread_members VALUES(5,5,7);
INSERT INTO thread_members VALUES(6,6,8);

INSERT INTO messages VALUES(NULL,'259478cf-64b3-44e1-9b1c-5d1773edc601',1,3,NULL,'Hi',1,1,1600074685,NULL,0);
INSERT INTO messages VALUES(NULL,'1a1cbd44-7526-4032-9665-45aee085ab65',1,3,NULL,'I''m fine',1,1,1600074789,NULL,0);
INSERT INTO messages VALUES(NULL,'af65adc0-2d80-4de8-83bb-9bf9ea4ebd5d',1,3,NULL,'How are you?',1,-1,1600074687,NULL,0);
INSERT INTO messages VALUES(NULL,'601f2a66-e6a6-4083-9dce-e5d78fb57520',2,4,NULL,'Get Unlimitted 5G',1,1,1600074800,NULL,0);
INSERT INTO messages VALUES(NULL,'271fe95c-5d47-4ffe-ae62-7f2f6b749711',2,4,NULL,'Get Unlimmtted 5G',1,1,1600074809,NULL,0);
INSERT INTO messages VALUES(NULL,'4dafafd9-734c-4f86-b1ec-09aa327b8a88',3,5,NULL,'Free unlimitted internet 4 99$',1,1,1600074802,NULL,0);
INSERT INTO messages VALUES(NULL,'1218070f-c820-40e1-bd33-5099d894683a',4,6,NULL,'Hello',1,1,1600075652,NULL,0);
INSERT INTO messages VALUES(NULL,'9abcc777-5b06-4570-9b83-48603a49add2',4,6,NULL,'Hi.',1,-1,1600075658,NULL,0);
INSERT INTO messages VALUES(NULL,'c5b99952-5517-4620-8f28-fb97f5017cee',6,8,NULL,'May I call you?',1,-1,1600075789,NULL,0);
INSERT INTO messages VALUES(NULL,'fe352125-1772-4360-831e-e2d56bb73c73',6,8,NULL,'Are you there?',1,-1,1600075790,NULL,0);
INSERT INTO messages VALUES(NULL,'c597bd6a-2e60-4df3-9c05-cc0c88861721',6,8,NULL,'OK. Call me later',1,-1,1600075791,NULL,0);
INSERT INTO messages VALUES(NULL,'f098e603-5ac1-4d5a-bcad-c7fe84c91252',6,8,NULL,'Sure, you may call me',1,1,1600075889,NULL,0);
INSERT INTO messages VALUES(NULL,'9d401342-3e30-4b25-859b-b56bd0ec2839',5,7,NULL,'SMS to India',1,-1,1600075909,NULL,0);
INSERT INTO messages VALUES(NULL,'776a3885-5cb1-41ed-9423-dfe3d2ac772a',5,7,NULL,'More SMS to India',1,-1,1600075913,NULL,0);

COMMIT;
