/*
  Tribe table, includes relevant id, url, and name.
*/
CREATE TABLE tribes (
    id BIGINT PRIMARY KEY,
    tribe_url TEXT,
    name TEXT
);
/*
  Character table
*/
CREATE TABLE characters (
    address BYTEA PRIMARY KEY, -- blockchain address
    name VARCHAR(255),
    id NUMERIC(78) UNIQUE,            
    tribe_id BIGINT REFERENCES tribes(id)  -- tribe_id may be nullable
);
/*
  Create systems table.
*/
CREATE TABLE IF NOT EXISTS systems
(
   id SERIAL PRIMARY KEY,
   region_id BIGINT,
   constellation_id BIGINT,
   solar_system_id BIGINT UNIQUE,
   solar_system_name TEXT,
   x DOUBLE PRECISION,
   y DOUBLE PRECISION,
   z DOUBLE PRECISION
);
/*
  Create verifiable incident table.
*/
CREATE TABLE incident (
    id BIGINT PRIMARY KEY,
    victim_id NUMERIC(78),
    killer_id NUMERIC(78),
    loss_type TEXT NOT NULL,
    solar_system_id BIGINT NOT NULL,
    time_stamp BIGINT NOT NULL,
    /*
      Added directly.
    */
    CONSTRAINT fk_victim_character FOREIGN KEY (victim_id) REFERENCES characters (id),
    CONSTRAINT fk_killer_character FOREIGN KEY (killer_id) REFERENCES characters (id),
    CONSTRAINT fk_incident_solar_system FOREIGN KEY (solar_system_id) REFERENCES systems (solar_system_id)
);
/*
  Payloard trigger for database updates.
*/
CREATE OR REPLACE FUNCTION notify_incident_trigger()
RETURNS trigger AS $$
DECLARE
    payload TEXT;
BEGIN
    -- Convert the new row to a JSON string
    payload := row_to_json(NEW)::text;
    -- Send the notification to the 'incident_trigger' channel with the payload
    PERFORM pg_notify('incident_trigger', payload);   
    -- Return the new row so that the insertion completes normally
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
/*
  Create a trigger.
*/
CREATE TRIGGER incident_notify_trigger
AFTER INSERT ON incident
FOR EACH ROW
EXECUTE FUNCTION notify_incident_trigger();
/*
  Precomputed index to speed up requests related to time filtration, UNIX format. Prior iterations had LDAP.
*/
CREATE INDEX idx_incident_converted_ts
ON incident (to_timestamp((time_stamp - 116444736000000000) / 10000000.0));
/*
  Precomputed index to speed up requests regarding name search for totals.
*/
CREATE INDEX idx_character_tribe_id ON characters(tribe_id);
CREATE INDEX idx_character_name ON characters(name);
CREATE INDEX idx_incident_killer_id ON incident (killer_id);
CREATE INDEX idx_incident_victim_id ON incident (victim_id);
CREATE INDEX idx_incident_solar_system_id ON incident (solar_system_id);
