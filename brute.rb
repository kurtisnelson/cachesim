L1_MAX_BYTES=41952
L2_MAX_BYTES=196608
BENCHMARK=ARGV[0]

CacheSpec = Struct.new(:c1, :b1, :s1, :c2, :b2, :s2, :k) do
  def valid?
    return false unless k.between?(0,4)
    return false unless c2 >= c1
    return false unless b2 >= b1
    return false unless s2 >= s1
    return false unless b1 > 0
    return false unless b2 > 0
    l1_lines = c1**2 / b1**2
    return false unless l1_lines >= 1
    l1_index_len = c1 - s1 - b1
    return false unless l1_index_len >= 1
    l1_tag_len = 64 - b1 - l1_index_len
    l1_size = (l1_lines * (b1**2 + l1_tag_len + 1))/8
    return false unless l1_size <= L1_MAX_BYTES
    l2_lines = c2**2 / b2**2
    return false unless l2_lines >= 1
    l2_index_len = c2 - s2 - b2
    return false unless l2_index_len >= 1
    l2_tag_len = 64 - b2 - l2_index_len
    l2_size = (l2_lines * (b2**2 + l2_tag_len + 1))/8
    return false unless l2_size <= L2_MAX_BYTES
    true
  end

  def to_s
    puts "C1: " + c1.to_s
    puts "B1: " + b1.to_s
    puts "S1: " + s1.to_s
    puts "C2: " + c2.to_s
    puts "B2: " + b2.to_s
    puts "S2: " + s2.to_s
    puts "K: " + k.to_s
  end
end

def random_cache
  loop do
    rnd_cache = CacheSpec.new(Random.rand(32) + 2, Random.rand(32) + 1, Random.rand(9), Random.rand(32) + 2, Random.rand(32) + 1, Random.rand(9), Random.rand(5))
    return rnd_cache if rnd_cache.valid?
  end
end

def permute_cache(c)
  c = c.clone
  loop do
    field = Random.rand(c.size)
    delta = Random.rand(4) + 1
    if Random.rand(2) == 0
      c[field] += delta if c[field] + delta <= 32
    else
      c[field] -= delta if c[field] - delta >= 0
    end
    return c if c.valid?
  end
end

def shell_run spec, trace
  IO.popen("./cachesim -c #{spec.c1} -b #{spec.b1} -s #{spec.s1} -C #{spec.c2} -B #{spec.b2} -S #{spec.s2} -k #{spec.k}", 'r+') do |ps|
    File.open(trace) do |f|
      f.each_line do |line|
        ps.write(line.chomp)
      end
      ps.close_write
    end
    aat_string = ps.readlines.last.chomp.split(' ').last
    return Float(aat_string)
  end
end

def save_best cache
  File.open("best/#{BENCHMARK}.best", 'w') do |file|
    file.write(cache.to_a.join(','))
  end
end

counter = 0
best_cache = CacheSpec.new(27,25,0,31,25,0,0)
File.open("best/#{BENCHMARK}.best", 'w+') do |f|
  params = f.readlines[0]
  break unless params
  params.split(',')
  (0..6).each do |i|
    best_cache[i] = params[i].to_i
  end
end
best_aat = shell_run(best_cache, "traces/#{BENCHMARK}.trace")

trap("SIGINT") do
  puts "Best:"
  puts best_cache.to_s
  save_best best_cache
  exit
end

while true
  puts counter
  if Random.rand(2) == 0
    puts "Permuting"
    cache = permute_cache(best_cache)
  else
    puts "Random"
    cache = random_cache if Random.rand(2)
  end
  begin
    puts "Running..."
    aat = shell_run(cache, "traces/#{BENCHMARK}.trace")
  rescue
    puts "BROKE " + $?.to_s
    puts cache.to_s
    aat = 999
  end

  if aat < best_aat
    puts "New Best Cache AAT: #{aat}"
    best_cache = cache
    best_aat = aat
    puts cache.to_s
  end
  counter += 1
  save_best best_cache if counter % 10 == 0
end
