Gem::Specification.new do |s|
  s.name = 'gctrack'
  s.version = '0.0.1'
  s.summary = 'Track Ruby GC events'
  s.description = <<-DOC
    This gem can be used to track Ruby GC tracepoints that are normally only visible through GC extensions.
  DOC
  s.homepage = 'https://github.com/Shopify/gctrack'
  s.authors = ['Scott Francis', 'Alex Snaps']
  s.email   = ['scott.francis@shopify.com', 'alex.snaps@shopify.com']
  s.license = 'MIT'

  s.files = `git ls-files`.split("\n")
  s.extensions = ['ext/gctrack/extconf.rb']
  s.add_development_dependency 'rake-compiler', '~> 1.0'
  s.add_development_dependency 'test-unit'
end
