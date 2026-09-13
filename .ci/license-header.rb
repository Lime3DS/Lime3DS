#!/usr/bin/ruby
# frozen_string_literal: true

YEARS_PLACEHOLDER = 'YEARS_PLACEHOLDER'

license_header_template = <<~EOF
      // Copyright #{YEARS_PLACEHOLDER} Citra Emulator Project / Azahar Emulator Project
      // Licensed under GPLv2 or any later version
      // Refer to the license.txt file included.
      EOF

def build_license_regex(template, placeholder)
   escaped_template = Regexp.escape(template)
   escaped_placeholder = Regexp.escape(placeholder)
   year_range_pattern = '(\d{4})(?:-(\d{4}))?'
   pattern = escaped_template.sub(escaped_placeholder, year_range_pattern)
   Regexp.new('\A' + pattern)
end

license_header_regex = build_license_regex(license_header_template, YEARS_PLACEHOLDER)

print 'Getting branch changes...'
branch_name = `git rev-parse --abbrev-ref HEAD`.chomp
branch_commits = `git log #{branch_name} --not master --pretty=format:"%h"`.split("\n")
branch_commit_range = "#{branch_commits[-1]}^..#{branch_commits[0]}"
branch_changed_files = `git diff-tree --no-commit-id --name-only #{branch_commit_range} -r`.split("\n")
puts 'done'

print 'Getting HEAD commit year...'
head_year = `git log -1 --format=%cd --date=format:%Y`.chomp.to_i
puts "done (#{head_year})"

print 'Checking files...'
missing_header_files = []
stale_year_files = []
branch_changed_files.each do |file_name|
   next unless file_name.end_with?('.cpp', '.cpp.in', '.h', '.h.in', '.inc', '.kt', '.kts', '.m', '.mm')
   next unless File.file?(file_name)

   file_content = File.read(file_name, mode: 'r:bom|utf-8')
   match = license_header_regex.match(file_content)

   if match.nil?
      missing_header_files.push(file_name)
      next
   end

   start_year = match[1].to_i
   end_year = (match[2] || match[1]).to_i
   unless (start_year..end_year).cover?(head_year)
      stale_year_files.push(file_name)
   end
end
puts 'done'

if missing_header_files.empty? && stale_year_files.empty?
   puts "\nAll changed files have correct headers"
   exit 0
end

unless missing_header_files.empty?
   puts <<-EOF

The following #{missing_header_files.length} files are missing a correct license header:
#{missing_header_files.join("\n")}

The following license header should be added to the start of all offending files (with a year or year range covering #{head_year}):
=== BEGIN ===
#{license_header_template.sub(YEARS_PLACEHOLDER, head_year.to_s)}
===  END  ===
   EOF
end

unless stale_year_files.empty?
   puts <<-EOF

The following #{stale_year_files.length} files have a license header whose year range does not cover #{head_year}:
#{stale_year_files.join("\n")}

Please update the copyright year range in these files to include #{head_year}.
   EOF
end

puts <<-EOF

If some of the code in this PR is not being contributed by the original author, the files which have been exclusively changed by that code can be ignored.
If this happens, this PR requirement can be bypassed once all other files are addressed.
EOF

exit 1
