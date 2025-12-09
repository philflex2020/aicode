


DROP TABLE IF EXISTS variable_pubs;
CREATE TABLE variable_pubs (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  variable_id INTEGER NOT NULL,
  variable_name TEXT NOT NULL,
  pub_name TEXT NOT NULL,
  format TEXT,                  -- 'json', 'simple', etc.
  scale DOUBLE,                  -- 'json', 'simple', etc.
  offset DOUBLE,                  -- 'json', 'simple', etc.
  extra TEXT,
  is_active INTEGER DEFAULT 1,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (variable_id) REFERENCES variables(id)
); 


DROP TABLE IF EXISTS variable_charts;
CREATE TABLE variable_charts (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  variable_id INTEGER NOT NULL,
  variable_name TEXT NOT NULL,
  chart_name TEXT,
  color TEXT,
  extra TEXT,
  format TEXT,
  scale DOUBLE,                  -- 'json', 'simple', etc.
  offset DOUBLE,                  -- 'json', 'simple', etc.
  is_active INTEGER DEFAULT 1,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (variable_id) REFERENCES variables(id)
);

DROP TABLE IF EXISTS variable_aggregates;
CREATE TABLE variable_aggregates (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  variable_id INTEGER NOT NULL,
  variable_name TEXT NOT NULL,
  agg_name TEXT,
  max_value DOUBLE,
  min_value DOUBLE,
  avg_value DOUBLE,
  count_value INTEGER,
  agg_reset INTEGER DEFAULT 0,
  is_active INTEGER DEFAULT 1,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (variable_id) REFERENCES variables(id)
);

DROP TABLE IF EXISTS variable_metrics;
CREATE TABLE variable_metrics (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  variable_id INTEGER NOT NULL,
  variable_name TEXT NOT NULL,
  metric_name TEXT NOT NULL,
  aggregation TEXT,   -- 'max','min','avg','count'
  extra TEXT,
  format TEXT,
  is_active INTEGER DEFAULT 1,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (variable_id) REFERENCES variables(id)
);

DROP TABLE IF EXISTS variable_prots;;
CREATE TABLE variable_prots (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  variable_id INTEGER NOT NULL,
  variable_name TEXT NOT NULL,
  prot_name TEXT,
  aggregation TEXT NOT NULL,   -- 'max','min','avg','count'
  hysteresis INTEGER DEFAULT 5,

  active_1  INTEGER DEFAULT 0,
  reset_1 INTEGER DEFAULT 0,

  prot_level_1 INTEGER,                  -- 1,2,3 .
  on_time_1 INTEGER DEFAULT 3,
  on_action_1 TEXT,
  on_count_1 INTEGER DEFAULT 0,

  off_time_1 INTEGER DEFAULT 3,
  off_action_1 TEXT,
  off_count_1 INTEGER DEFAULT 0,

  active_2  INTEGER DEFAULT 0,
  reset_2 INTEGER DEFAULT 0,

  prot_level_2 INTEGER,                  -- 1,2,3 .
  on_time_2 INTEGER DEFAULT 3,
  on_action_2 TEXT,
  on_count_2 INTEGER DEFAULT 0,

  off_time_2 INTEGER DEFAULT 3,
  off_action_2 TEXT,
  off_count_2 INTEGER DEFAULT 0,

  active_3  INTEGER DEFAULT 0,
  reset_3 INTEGER DEFAULT 0,

  prot_level_3 INTEGER,                  -- 1,2,3 .
  on_time_3 INTEGER DEFAULT 3,
  on_action_3 TEXT,
  on_count_3 INTEGER DEFAULT 0,

  off_time_3 INTEGER DEFAULT 3,
  off_action_3 TEXT,
  off_count_3 INTEGER DEFAULT 0,

  extra TEXT,
  format TEXT,
  is_active INTEGER DEFAULT 0,
  is_acked INTEGER DEFAULT 0,
  is_reset INTEGER DEFAULT 0,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (variable_id) REFERENCES variables(id)
);