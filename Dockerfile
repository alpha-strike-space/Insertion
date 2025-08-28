FROM postgres:latest

COPY ./schema.sql /docker-entrypoint-initdb.d

EXPOSE 5432

RUN chmod a+r /docker-entrypoint-initdb.d/*
