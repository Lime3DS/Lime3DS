#!/usr/bin/ruby
# frozen_string_literal: true

license_header = <<~EOF
      // Copyright Citra Emulator Project / Lime3DS Emulator Project
      // Licensed under GPLv2 or any later version
      // Refer to the license.txt file included.
      EOF

print 'Getting branch changes...'
branch_name = `git rev-parse --abbrev-ref HEAD`.chomp
branch_commits = `git log #{branch_name} --not master --pretty=format:"%h"`.split("\n")
branch_commit_range = "#{branch_commits[-1]}^..#{branch_commits[0]}"
branch_changed_files = `git diff-tree --no-commit-id --name-only #{branch_commit_range} -r`.split("\n")
puts 'done'

print 'Checking files...'
issue_files = []
branch_changed_files.each do |file_name|
   if file_name.end_with?('.cpp', '.h', '.kt') and File.file?(file_name)
      file_content = File.read(file_name)
      if not file_content.start_with?(license_header)
         issue_files.push(file_name)
      end
   end
end
puts 'done'

if issue_files.empty?
   puts "\nAll changed files have correct headers"
   exit 0
end

puts <<-EOF

The following #{issue_files.length} files have incorrect license headers:
#{issue_files.join("\n")}

The following license header should be added to the start of all offending files:
=== BEGIN ===
#{license_header}
===  END  ===

If some of the code in this PR is not being contributed by the original author, the files which have been exclusively changed by that code can be ignored.
If this happens, this PR requirement can be bypassed once all other files are addressed.
EOF

exit 1
