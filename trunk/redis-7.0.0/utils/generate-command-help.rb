#!/usr/bin/env ruby -w
# Usage: generate-command-help.r [path/to/commands.json]
#    or: generate-commands-json.py | generate-command-help.rb -
#
# Defaults to downloading commands.json from the redis-doc repo if not provided
# or STDINed.

GROUPS = [
  "generic",
  "string",
  "list",
  "set",
  "sorted-set",
  "hash",
  "pubsub",
  "transactions",
  "connection",
  "server",
  "scripting",
  "hyperloglog",
  "cluster",
  "geo",
  "stream",
  "bitmap"
].freeze

GROUPS_BY_NAME = Hash[*
  GROUPS.each_with_index.map do |n,i|
    [n,i]
  end.flatten
].freeze

def argument arg
  if "block" == arg["type"]
    name = arg["arguments"].map do |entry|
      argument entry
    end.join " "
  elsif "oneof" == arg["type"]
    name = arg["arguments"].map do |entry|
      argument entry
    end.join "|"
  elsif "pure-token" == arg["type"]
    name = nil    # prepended later
  else
    name = arg["name"].is_a?(Array) ? arg["name"].join(" ") : arg["name"]
  end
  if arg["multiple"]
    name = "#{name} [#{name} ...]"
  end
  if arg["token"]
    name = [arg["token"], name].compact.join " "
  end
  if arg["optional"]
    name = "[#{name}]"
  end
  name
end

def arguments command
  return "" unless command["arguments"]
  command["arguments"].map do |arg|
    argument arg
  end.join " "
end

def commands
  return @commands if @commands

  require "rubygems"
  require "net/http"
  require "net/https"
  require "json"
  require "uri"
  if ARGV.length > 0
    if ARGV[0] == '-'
      data = STDIN.read
    elsif FileTest.exist? ARGV[0]
      data = File.read(ARGV[0])
    else
      raise Exception.new "File not found: #{ARGV[0]}"
    end
  else
    url = URI.parse "https://raw.githubusercontent.com/redis/redis-doc/master/commands.json"
    client = Net::HTTP.new url.host, url.port
    client.use_ssl = true
    response = client.get url.path
    if !response.is_a?(Net::HTTPSuccess)
      response.error!
      return
    else
      data = response.body
    end
  end
  @commands = JSON.parse(data)
end

def generate_groups
  GROUPS.map do |n|
    "\"#{n}\""
  end.join(",\n    ");
end

def generate_commands
  commands.to_a.sort do |x,y|
    x[0] <=> y[0]
  end.map do |key, command|
    group = GROUPS_BY_NAME[command["group"]]
    if group.nil?
      STDERR.puts "Please update groups array in #{__FILE__}"
      raise "Unknown group #{command["group"]}"
    end

    ret = <<-SPEC
{ "#{key}",
    "#{arguments(command)}",
    "#{command["summary"]}",
    #{group},
    "#{command["since"]}" }
    SPEC
    ret.strip
  end.join(",\n    ")
end

# Write to stdout
puts <<-HELP_H
/* Automatically generated by #{__FILE__}, do not edit. */

#ifndef __REDIS_HELP_H
#define __REDIS_HELP_H

static char *commandGroups[] = {
    #{generate_groups}
};

struct commandHelp {
  char *name;
  char *params;
  char *summary;
  int group;
  char *since;
} commandHelp[] = {
    #{generate_commands}
};

#endif
HELP_H

